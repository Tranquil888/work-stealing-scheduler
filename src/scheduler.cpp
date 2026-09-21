#include <stdexcept>
#include <wss/scheduler.hpp>

namespace wss {

namespace {

std::size_t resolve_workers(std::size_t n) {
    if (n != 0) return n;
    unsigned hw = std::thread::hardware_concurrency();
    return hw == 0 ? 4 : hw;
}

bool all_queues_empty(const std::vector<std::unique_ptr<Worker>>& workers) {
    for (auto& w : workers) {
        if (!w->empty()) return false;
    }
    return true;
}

}  // namespace

Scheduler::Scheduler(std::size_t worker_count)
    : Scheduler([](std::size_t n) {
          SchedulerConfig c;
          c.worker_count = n;
          return c;
      }(worker_count)) {}

Scheduler::Scheduler(SchedulerConfig config) : config_(std::move(config)) {
    config_.worker_count = resolve_workers(config_.worker_count);
    victim_policy_ = make_victim_policy(config_.victim_policy);
    if (config_.lifo) {
        ordering_policy_ = std::make_unique<LifoPolicy>();
    } else {
        ordering_policy_ = std::make_unique<FifoPolicy>();
    }
    workers_.reserve(config_.worker_count);
    for (std::size_t i = 0; i < config_.worker_count; ++i) {
        workers_.push_back(std::make_unique<Worker>(
            i, make_queue(config_.queue_kind, ordering_policy_->lifo())));
    }
    steal_ctx_.policy = victim_policy_.get();
    steal_ctx_.stop = &stop_;
    steal_ctx_.observer = observer_ ? observer_.get() : nullptr;
    steal_ctx_.queues.reserve(workers_.size());
    for (auto& w : workers_) steal_ctx_.queues.push_back(w->queue());
    for (auto& w : workers_) w->start(&steal_ctx_);
}

Scheduler::~Scheduler() { shutdown(true); }

TaskHandle Scheduler::submit_task(TaskPtr task) {
    if (!task) return TaskHandle{};
    if (shutdown_called_.load(std::memory_order_acquire)) {
        throw std::runtime_error("wss: submit on stopped scheduler");
    }
    TaskHandle handle{task};
    route(std::move(task));
    return handle;
}

void Scheduler::route(TaskPtr task) {
    std::size_t n = workers_.size();
    if (n == 0) return;
    std::size_t idx = next_queue_.fetch_add(1, std::memory_order_relaxed) % n;
    workers_[idx]->post_external(std::move(task));
}

void Scheduler::ensure_timer() {
    std::lock_guard<std::mutex> lock(timer_mutex_);
    if (timer_) return;
    if (shutdown_called_.load(std::memory_order_acquire)) {
        throw std::runtime_error("wss: timer on stopped scheduler");
    }
    timer_ = std::make_shared<TimerService>([this](std::function<void()> fn) {
        submit_task(std::make_shared<FunctionTask>(std::move(fn)));
    });
}

void Scheduler::shutdown(bool drain) {
    bool expected = false;
    if (!shutdown_called_.compare_exchange_strong(expected, true)) return;
    {
        std::lock_guard<std::mutex> lock(timer_mutex_);
        if (timer_) timer_->shutdown();
    }
    if (drain) {
        for (int i = 0; i < 5000; ++i) {
            if (all_queues_empty(workers_)) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    stop_.store(true, std::memory_order_release);
    for (auto& w : workers_) w->notify();
    for (auto& w : workers_) w->join();
    TaskPtr leftover;
    for (auto& w : workers_) {
        while (w->try_take_inbox(leftover) || w->try_take_local(leftover)) {
            if (drain) {
                ITaskObserver* obs = steal_ctx_.observer;
                if (obs != nullptr) obs->on_start(leftover->id(), leftover->kind());
                leftover->execute();
                if (obs != nullptr) {
                    obs->on_finish(leftover->id(), leftover->kind(),
                                   leftover->state(),
                                   std::chrono::nanoseconds{0});
                }
            } else {
                leftover->cancel();
            }
            leftover.reset();
        }
    }
}

bool Scheduler::wait_idle(std::chrono::milliseconds timeout) const {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    // Two consecutive empty snapshots so a task in flight between a pop and a
    // push is not mistaken for quiescence.
    int calm = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        if (all_queues_empty(workers_)) {
            if (++calm >= 2) return true;
        } else {
            calm = 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return all_queues_empty(workers_);
}

MetricsSnapshot Scheduler::metrics() const {
    auto* m = dynamic_cast<MetricsCollector*>(observer_.get());
    if (m != nullptr) return m->snapshot();
    return MetricsSnapshot{};
}

void Scheduler::set_observer(std::shared_ptr<ITaskObserver> observer) {
    observer_ = std::move(observer);
    steal_ctx_.observer = observer_ ? observer_.get() : nullptr;
}

}  // namespace wss
