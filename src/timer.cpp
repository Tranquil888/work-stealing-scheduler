#include <wss/timer.hpp>

namespace wss {

TimerHandle::TimerHandle(std::weak_ptr<TimerService> owner, std::uint64_t id)
    : owner_(std::move(owner)), id_(id) {}

bool TimerHandle::valid() const noexcept {
    return id_ != 0 && !owner_.expired();
}

bool TimerHandle::cancel() {
    auto svc = owner_.lock();
    if (!svc) return false;
    return svc->cancel(id_);
}

TimerService::TimerService(SubmitFn submit) : submit_(std::move(submit)) {
    thread_ = std::thread([this] { loop(); });
    started_ = true;
}

TimerService::~TimerService() { shutdown(); }

TimerHandle TimerService::schedule_after(std::chrono::nanoseconds delay,
                                         std::function<void()> func) {
    Entry e;
    e.due = std::chrono::steady_clock::now() + delay;
    e.func = std::move(func);
    return insert(std::move(e));
}

TimerHandle TimerService::schedule_every(std::chrono::nanoseconds interval,
                                         std::function<void()> func) {
    Entry e;
    e.due = std::chrono::steady_clock::now() + interval;
    e.interval = interval;
    e.func = std::move(func);
    return insert(std::move(e));
}

TimerHandle TimerService::insert(Entry entry) {
    entry.id = next_id_.fetch_add(1, std::memory_order_relaxed);
    TimerHandle h(shared_from_this(), entry.id);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        heap_.push(std::move(entry));
    }
    cv_.notify_one();
    return h;
}

bool TimerService::cancel(std::uint64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto [it, inserted] = cancelled_.emplace(id, true);
    (void)it;
    bool fresh = inserted;
    cv_.notify_one();
    return fresh;
}

void TimerService::shutdown() {
    bool expected = false;
    if (!stop_.compare_exchange_strong(expected, true)) {
        if (started_ && thread_.joinable() &&
            thread_.get_id() != std::this_thread::get_id()) {
            thread_.join();
        }
        return;
    }
    cv_.notify_all();
    if (started_ && thread_.joinable() &&
        thread_.get_id() != std::this_thread::get_id()) {
        thread_.join();
    }
}

void TimerService::loop() {
    while (!stop_.load(std::memory_order_acquire)) {
        std::function<void()> fire;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (heap_.empty()) {
                cv_.wait(lock, [this] {
                    return stop_.load(std::memory_order_acquire) || !heap_.empty();
                });
                continue;
            }
            const Entry& top = heap_.top();
            if (cancelled_.erase(top.id) > 0) {
                heap_.pop();
                continue;
            }
            auto now = std::chrono::steady_clock::now();
            if (top.due > now) {
                cv_.wait_until(lock, top.due, [this] {
                    return stop_.load(std::memory_order_acquire);
                });
                continue;
            }
            Entry due = top;
            heap_.pop();
            if (due.interval.count() > 0) {
                auto tick = due.interval;
                do {
                    due.due += tick;
                } while (due.due <= now);
                if (cancelled_.erase(due.id) == 0) heap_.push(due);
            }
            fire = std::move(due.func);
        }
        if (fire) {
            try {
                submit_(std::move(fire));
            } catch (...) {
                // Scheduler is gone; nowhere left to submit to.
            }
        }
    }
}

}  // namespace wss
