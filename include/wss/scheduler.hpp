#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

#include <wss/cancellation.hpp>
#include <wss/executor.hpp>
#include <wss/future.hpp>
#include <wss/observer.hpp>
#include <wss/scheduling_policy.hpp>
#include <wss/shared_state.hpp>
#include <wss/task.hpp>
#include <wss/task_handle.hpp>
#include <wss/timer.hpp>
#include <wss/work_queue.hpp>
#include <wss/work_stealing_policy.hpp>
#include <wss/worker.hpp>

namespace wss {

struct SchedulerConfig {
    std::size_t worker_count{0};
    QueueKind queue_kind{QueueKind::ChaseLev};
    VictimPolicyKind victim_policy{VictimPolicyKind::Random};
    bool lifo{true};
    bool start_timer{true};
};

class Scheduler final : public IExecutor {
public:
    explicit Scheduler(std::size_t worker_count);
    explicit Scheduler(SchedulerConfig config);
    ~Scheduler() override;

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    TaskHandle submit_task(TaskPtr task) override;

    template <typename F>
    auto submit(F&& func) -> Future<std::invoke_result_t<F>> {
        using R = std::invoke_result_t<F>;
        auto state = std::make_shared<SharedState<R>>();
        Future<R> fut(state);
        auto task = std::make_shared<FutureTask<R>>(
            std::function<R()>(std::forward<F>(func)), state);
        submit_task(task);
        return fut;
    }

    template <typename F>
    auto submit_with_token(F&& func, CancellationToken tok)
        -> Future<std::invoke_result_t<F>> {
        using R = std::invoke_result_t<F>;
        auto state = std::make_shared<SharedState<R>>();
        Future<R> fut(state);
        auto task = std::make_shared<FutureTask<R>>(
            std::function<R()>(std::forward<F>(func)), state, tok);
        submit_task(task);
        return fut;
    }

    template <typename F>
    auto submit(F&& func, CancellationToken tok)
        -> Future<std::invoke_result_t<F>> {
        return submit_with_token(std::forward<F>(func), tok);
    }

    // Fire-and-forget handles come from submit_task; submit always returns
    // a Future. The scheduler must outlive continuations registered via
    // Future::then, and set_observer must precede the first submit.

    template <typename F>
    TimerHandle schedule_after(std::chrono::nanoseconds delay, F&& func) {
        ensure_timer();
        std::function<void()> fn(std::forward<F>(func));
        return timer_->schedule_after(delay, [this, fn = std::move(fn)] {
            submit_task(std::make_shared<TimerTask>(fn));
        });
    }

    template <typename F>
    TimerHandle schedule_every(std::chrono::nanoseconds interval, F&& func) {
        ensure_timer();
        std::function<void()> fn(std::forward<F>(func));
        return timer_->schedule_every(interval, [this, fn = std::move(fn)] {
            submit_task(std::make_shared<TimerTask>(fn));
        });
    }

    void shutdown(bool drain = true);
    // Empty queues twice in a row, not a guarantee that bodies already
    // running have finished. Join on futures when that matters.
    bool wait_idle(std::chrono::milliseconds timeout) const;
    std::size_t worker_count() const noexcept { return workers_.size(); }
    // Zeros unless a MetricsCollector was installed via set_observer.
    MetricsSnapshot metrics() const;

    void set_observer(std::shared_ptr<ITaskObserver> observer);

private:
    void ensure_timer();
    void route(TaskPtr task);

    SchedulerConfig config_;
    std::vector<std::unique_ptr<Worker>> workers_;
    std::unique_ptr<IWorkStealingPolicy> victim_policy_;
    std::unique_ptr<ISchedulingPolicy> ordering_policy_;
    std::shared_ptr<ITaskObserver> observer_;
    std::shared_ptr<TimerService> timer_;
    std::mutex timer_mutex_;
    StealContext steal_ctx_;
    std::atomic<bool> stop_{false};
    std::atomic<std::size_t> next_queue_{0};
    std::atomic<bool> shutdown_called_{false};
};

}  // namespace wss
