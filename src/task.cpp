#include <wss/task.hpp>

namespace wss {

std::atomic<std::uint64_t> TaskBase::next_id_{1};

TaskBase::TaskBase(CancellationToken token, const char* kind)
    : id_(next_id_.fetch_add(1, std::memory_order_relaxed)),
      kind_(kind),
      token_(std::move(token)) {}

bool TaskBase::cancel() {
    TaskState expected = TaskState::Pending;
    return state_.compare_exchange_strong(expected, TaskState::Cancelled,
                                          std::memory_order_acq_rel);
}

bool TaskBase::begin_run() noexcept {
    TaskState expected = TaskState::Pending;
    return state_.compare_exchange_strong(expected, TaskState::Running,
                                          std::memory_order_acq_rel);
}

void TaskBase::end_run(TaskState terminal) noexcept {
    state_.store(terminal, std::memory_order_release);
}

FunctionTask::FunctionTask(std::function<void()> func, CancellationToken token)
    : TaskBase(std::move(token), "function"), func_(std::move(func)) {}

void FunctionTask::execute() {
    if (is_cancellation_requested()) cancel();
    if (!begin_run()) return;
    if (is_cancellation_requested()) {
        end_run(TaskState::Cancelled);
        return;
    }
    try {
        func_();
        end_run(TaskState::Completed);
    } catch (...) {
        end_run(TaskState::Failed);
    }
}

ContinuationTask::ContinuationTask(std::function<void()> func,
                                   CancellationToken token)
    : TaskBase(std::move(token), "continuation"), func_(std::move(func)) {}

void ContinuationTask::execute() {
    if (is_cancellation_requested()) cancel();
    if (!begin_run()) return;
    if (is_cancellation_requested()) {
        end_run(TaskState::Cancelled);
        return;
    }
    try {
        func_();
        end_run(TaskState::Completed);
    } catch (...) {
        end_run(TaskState::Failed);
    }
}

TimerTask::TimerTask(std::function<void()> func, CancellationToken token)
    : TaskBase(std::move(token), "timer"), func_(std::move(func)) {}

void TimerTask::execute() {
    if (is_cancellation_requested()) cancel();
    if (!begin_run()) return;
    if (is_cancellation_requested()) {
        end_run(TaskState::Cancelled);
        return;
    }
    try {
        func_();
        end_run(TaskState::Completed);
    } catch (...) {
        end_run(TaskState::Failed);
    }
}

}  // namespace wss
