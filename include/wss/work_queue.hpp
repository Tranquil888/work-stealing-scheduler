#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

#include <wss/task.hpp>

namespace wss {

enum class QueueKind { Mutex, ChaseLev };

class IWorkQueue {
public:
    virtual ~IWorkQueue() = default;

    virtual bool push(TaskPtr task) = 0;
    virtual bool try_pop(TaskPtr& out) = 0;
    virtual bool try_steal(TaskPtr& out) = 0;
    virtual std::size_t size() const = 0;
    virtual bool empty() const = 0;
    virtual void clear() = 0;
};

class MutexQueue final : public IWorkQueue {
public:
    explicit MutexQueue(bool lifo = false);

    bool push(TaskPtr task) override;
    bool try_pop(TaskPtr& out) override;
    bool try_steal(TaskPtr& out) override;
    std::size_t size() const override;
    bool empty() const override;
    void clear() override;

private:
    mutable std::mutex mutex_;
    std::deque<TaskPtr> deque_;
    bool lifo_;
};

class ChaseLevDeque final : public IWorkQueue {
public:
    static constexpr std::size_t kCapacity = 8192;

    ChaseLevDeque();
    ~ChaseLevDeque() override;

    ChaseLevDeque(const ChaseLevDeque&) = delete;
    ChaseLevDeque& operator=(const ChaseLevDeque&) = delete;

    bool push(TaskPtr task) override;
    bool try_pop(TaskPtr& out) override;
    bool try_steal(TaskPtr& out) override;
    std::size_t size() const override;
    bool empty() const override;
    void clear() override;

private:
    std::vector<TaskPtr> slots_;
    alignas(64) std::atomic<std::int64_t> top_{0};
    alignas(64) std::atomic<std::int64_t> bottom_{0};
};

std::unique_ptr<IWorkQueue> make_queue(QueueKind kind, bool lifo = false);

}  // namespace wss
