#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>

#include <wss/cancellation.hpp>

namespace wss {

enum class TaskState : std::uint8_t { Pending, Running, Completed, Cancelled, Failed };

constexpr std::string_view to_string(TaskState s) noexcept {
    switch (s) {
        case TaskState::Pending: return "pending";
        case TaskState::Running: return "running";
        case TaskState::Completed: return "completed";
        case TaskState::Cancelled: return "cancelled";
        case TaskState::Failed: return "failed";
    }
    return "unknown";
}

class ITask {
public:
    virtual ~ITask() = default;

    virtual std::uint64_t id() const noexcept = 0;
    virtual TaskState state() const noexcept = 0;
    virtual const char* kind() const noexcept = 0;
    virtual void execute() = 0;
    virtual bool cancel() = 0;
    virtual bool is_cancellation_requested() const noexcept = 0;
};

using TaskPtr = std::shared_ptr<ITask>;

class TaskBase : public ITask {
public:
    explicit TaskBase(CancellationToken token = CancellationToken::none(),
                      const char* kind = "task");
    ~TaskBase() override = default;

    TaskBase(const TaskBase&) = delete;
    TaskBase& operator=(const TaskBase&) = delete;

    std::uint64_t id() const noexcept override { return id_; }
    TaskState state() const noexcept override {
        return state_.load(std::memory_order_acquire);
    }
    const char* kind() const noexcept override { return kind_; }
    bool cancel() override;
    bool is_cancellation_requested() const noexcept override {
        return token_.is_cancelled();
    }

protected:
    bool begin_run() noexcept;
    void end_run(TaskState terminal) noexcept;
    CancellationToken token() const { return token_; }

private:
    static std::atomic<std::uint64_t> next_id_;

    std::uint64_t id_;
    const char* kind_;
    CancellationToken token_;
    std::atomic<TaskState> state_{TaskState::Pending};
};

class FunctionTask final : public TaskBase {
public:
    explicit FunctionTask(std::function<void()> func,
                          CancellationToken token = CancellationToken::none());

    void execute() override;

private:
    std::function<void()> func_;
};

class ContinuationTask final : public TaskBase {
public:
    explicit ContinuationTask(std::function<void()> func,
                              CancellationToken token = CancellationToken::none());

    void execute() override;

private:
    std::function<void()> func_;
};

class TimerTask final : public TaskBase {
public:
    explicit TimerTask(std::function<void()> func,
                       CancellationToken token = CancellationToken::none());

    void execute() override;

private:
    std::function<void()> func_;
};

}  // namespace wss
