#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <wss/task.hpp>
#include <wss/work_queue.hpp>

namespace wss {

class IWorkStealingPolicy;
class ITaskObserver;

struct StealContext {
    std::vector<IWorkQueue*> queues{};
    IWorkStealingPolicy* policy{nullptr};
    std::atomic<bool>* stop{nullptr};
    ITaskObserver* observer{nullptr};
};

class Worker {
public:
    Worker(std::size_t id, std::unique_ptr<IWorkQueue> queue);
    ~Worker();

    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    void start(StealContext* ctx);
    void stop_request();
    void join();
    void notify();

    // Thread-safe: this is how other threads hand work to this worker.
    // External pushes land in the inbox; only the worker thread moves them
    // into its deque, which keeps the Chase-Lev push/pop single-owner.
    void post_external(TaskPtr task);

    bool empty() const;
    bool try_take_inbox(TaskPtr& out);
    bool try_take_local(TaskPtr& out);

    std::size_t id() const noexcept { return id_; }
    IWorkQueue* queue() const noexcept { return queue_.get(); }
    bool running() const noexcept { return running_.load(std::memory_order_acquire); }

private:
    void run(StealContext* ctx);
    void drain_inbox();
    bool pull_local(TaskPtr& out);
    bool steal(StealContext* ctx, TaskPtr& out);
    void execute_one(const TaskPtr& task, ITaskObserver* observer);

    std::size_t id_;
    std::unique_ptr<IWorkQueue> queue_;
    MutexQueue inbox_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_local_{false};
    std::atomic<bool> running_{false};
    StealContext* ctx_{nullptr};
};

}  // namespace wss
