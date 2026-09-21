#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <wss/task.hpp>

namespace wss {

class IWorkQueue;
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

    std::size_t id() const noexcept { return id_; }
    IWorkQueue* queue() const noexcept { return queue_.get(); }
    bool running() const noexcept { return running_.load(std::memory_order_acquire); }

private:
    void run(StealContext* ctx);
    bool pull_local(TaskPtr& out);
    bool steal(StealContext* ctx, TaskPtr& out);
    void execute_one(const TaskPtr& task, ITaskObserver* observer);

    std::size_t id_;
    std::unique_ptr<IWorkQueue> queue_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_local_{false};
    std::atomic<bool> running_{false};
    StealContext* ctx_{nullptr};
};

}  // namespace wss
