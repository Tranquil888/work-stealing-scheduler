#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

#include <wss/cancellation.hpp>

namespace wss {

class TimerService;

class TimerHandle {
public:
    TimerHandle() = default;

    bool valid() const noexcept;
    bool cancel();
    std::uint64_t id() const noexcept { return id_; }

private:
    friend class TimerService;

    TimerHandle(std::weak_ptr<TimerService> owner, std::uint64_t id);

    std::weak_ptr<TimerService> owner_;
    std::uint64_t id_{0};
};

class ITimerService {
public:
    virtual ~ITimerService() = default;

    virtual TimerHandle schedule_after(std::chrono::nanoseconds delay,
                                       std::function<void()> func) = 0;
    virtual TimerHandle schedule_every(std::chrono::nanoseconds interval,
                                       std::function<void()> func) = 0;
    virtual bool cancel(std::uint64_t id) = 0;
    virtual void shutdown() = 0;
};

class TimerService final : public ITimerService,
                           public std::enable_shared_from_this<TimerService> {
public:
    using SubmitFn = std::function<void(std::function<void()>)>;

    explicit TimerService(SubmitFn submit);
    ~TimerService() override;

    TimerService(const TimerService&) = delete;
    TimerService& operator=(const TimerService&) = delete;

    TimerHandle schedule_after(std::chrono::nanoseconds delay,
                               std::function<void()> func) override;
    TimerHandle schedule_every(std::chrono::nanoseconds interval,
                               std::function<void()> func) override;
    bool cancel(std::uint64_t id) override;
    void shutdown() override;

private:
    struct Entry {
        std::chrono::steady_clock::time_point due;
        std::chrono::nanoseconds interval{0};
        std::uint64_t id{0};
        std::function<void()> func;

        bool operator>(const Entry& other) const { return due > other.due; }
    };

    void loop();
    TimerHandle insert(Entry entry);

    SubmitFn submit_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> heap_;
    std::unordered_map<std::uint64_t, bool> cancelled_;
    std::atomic<std::uint64_t> next_id_{1};
    std::atomic<bool> stop_{false};
    bool started_{false};
    std::thread thread_;
};

}  // namespace wss
