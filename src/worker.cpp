#include <wss/observer.hpp>
#include <wss/work_queue.hpp>
#include <wss/work_stealing_policy.hpp>
#include <wss/worker.hpp>

namespace wss {

Worker::Worker(std::size_t id, std::unique_ptr<IWorkQueue> queue)
    : id_(id), queue_(std::move(queue)) {}

Worker::~Worker() {
    stop_request();
    join();
}

void Worker::start(StealContext* ctx) {
    ctx_ = ctx;
    stop_local_.store(false, std::memory_order_release);
    thread_ = std::thread([this, ctx] { run(ctx); });
}

void Worker::stop_request() {
    stop_local_.store(true, std::memory_order_release);
    notify();
}

void Worker::join() {
    if (thread_.joinable()) thread_.join();
}

void Worker::notify() {
    std::lock_guard<std::mutex> lock(mutex_);
    cv_.notify_one();
}

void Worker::post_external(TaskPtr task) {
    if (!task) return;
    inbox_.push(std::move(task));
    notify();
}

bool Worker::empty() const {
    return inbox_.empty() && queue_->empty();
}

bool Worker::try_take_inbox(TaskPtr& out) { return inbox_.try_pop(out); }

bool Worker::try_take_local(TaskPtr& out) {
    return queue_->try_pop(out) || queue_->try_steal(out);
}

void Worker::drain_inbox() {
    TaskPtr task;
    while (inbox_.try_pop(task)) {
        if (!queue_->push(task)) {
            inbox_.push(task);
            return;
        }
        task.reset();
    }
}

void Worker::run(StealContext* ctx) {
    running_.store(true, std::memory_order_release);
    TaskPtr task;
    while (!stop_local_.load(std::memory_order_acquire) &&
           !ctx->stop->load(std::memory_order_acquire)) {
        drain_inbox();
        if (pull_local(task)) {
            execute_one(task, ctx->observer);
            task.reset();
            continue;
        }
        if (steal(ctx, task)) {
            execute_one(task, ctx->observer);
            task.reset();
            continue;
        }
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, std::chrono::milliseconds(2), [this, ctx] {
            return stop_local_.load(std::memory_order_acquire) ||
                   ctx->stop->load(std::memory_order_acquire);
        });
    }
    running_.store(false, std::memory_order_release);
}

bool Worker::pull_local(TaskPtr& out) { return queue_->try_pop(out); }

bool Worker::steal(StealContext* ctx, TaskPtr& out) {
    std::size_t count = ctx->queues.size();
    if (count < 2 || ctx->policy == nullptr) return false;
    for (int attempt = 0; attempt < static_cast<int>(count) * 2; ++attempt) {
        std::size_t victim = ctx->policy->pick_victim(id_, count, attempt);
        if (victim == id_ || victim >= count) continue;
        if (ctx->queues[victim]->try_steal(out)) {
            if (ctx->observer != nullptr) {
                ctx->observer->on_steal(out ? out->id() : 0, victim, id_);
            }
            return true;
        }
    }
    return false;
}

void Worker::execute_one(const TaskPtr& task, ITaskObserver* observer) {
    if (!task) return;
    auto start = std::chrono::steady_clock::now();
    if (observer != nullptr) observer->on_start(task->id(), task->kind());
    task->execute();
    if (observer != nullptr) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        observer->on_finish(task->id(), task->kind(), task->state(), elapsed);
    }
}

}  // namespace wss
