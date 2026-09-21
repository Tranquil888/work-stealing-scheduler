#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>

#include <wss/task.hpp>

namespace wss {

struct MetricsSnapshot {
    std::uint64_t started{0};
    std::uint64_t finished{0};
    std::uint64_t stolen{0};
    std::uint64_t cancelled{0};
    std::uint64_t failed{0};
    std::uint64_t total_ns{0};
};

class ITaskObserver {
public:
    virtual ~ITaskObserver() = default;
    virtual void on_start(std::uint64_t id, const char* kind) = 0;
    virtual void on_finish(std::uint64_t id, const char* kind, TaskState state,
                           std::chrono::nanoseconds elapsed) = 0;
    virtual void on_steal(std::uint64_t, std::size_t, std::size_t) {}
};

class NullObserver final : public ITaskObserver {
public:
    void on_start(std::uint64_t, const char*) override {}
    void on_finish(std::uint64_t, const char*, TaskState,
                   std::chrono::nanoseconds) override {}
};

class MetricsCollector final : public ITaskObserver {
public:
    void on_start(std::uint64_t, const char*) override;
    void on_finish(std::uint64_t, const char*, TaskState,
                   std::chrono::nanoseconds elapsed) override;
    void on_steal(std::uint64_t, std::size_t, std::size_t) override;

    MetricsSnapshot snapshot() const;

private:
    mutable std::mutex mutex_;
    MetricsSnapshot data_;
};

class LogObserver final : public ITaskObserver {
public:
    explicit LogObserver(FILE* out = stderr);

    void on_start(std::uint64_t id, const char* kind) override;
    void on_finish(std::uint64_t id, const char* kind, TaskState state,
                   std::chrono::nanoseconds elapsed) override;

private:
    FILE* out_;
    std::mutex mutex_;
};

}  // namespace wss
