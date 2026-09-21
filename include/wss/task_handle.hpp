#pragma once

#include <cstdint>
#include <memory>

#include <wss/task.hpp>

namespace wss {

class TaskHandle {
public:
    TaskHandle() = default;
    explicit TaskHandle(TaskPtr task) : task_(std::move(task)) {}

    bool valid() const noexcept { return static_cast<bool>(task_); }
    explicit operator bool() const noexcept { return valid(); }

    std::uint64_t id() const noexcept { return task_ ? task_->id() : 0; }

    TaskState state() const noexcept {
        return task_ ? task_->state() : TaskState::Cancelled;
    }

    bool cancel() { return task_ && task_->cancel(); }

    TaskPtr get() const noexcept { return task_; }

    friend bool operator==(const TaskHandle& a, const TaskHandle& b) noexcept {
        return a.task_ == b.task_;
    }
    friend bool operator!=(const TaskHandle& a, const TaskHandle& b) noexcept {
        return !(a == b);
    }

private:
    TaskPtr task_;
};

}  // namespace wss
