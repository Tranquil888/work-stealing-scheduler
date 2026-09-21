#include <atomic>
#include <chrono>
#include <vector>

#include <wss/observer.hpp>
#include <wss/scheduler.hpp>

#include "test_framework.hpp"

WSS_TEST(Scheduler, RunsSubmittedTasks) {
    wss::Scheduler pool(4);
    std::atomic<int> n{0};
    std::vector<wss::Future<int>> futs;
    for (int i = 0; i < 100; ++i) {
        futs.push_back(pool.submit([&n, i] {
            n.fetch_add(1, std::memory_order_relaxed);
            return i;
        }));
    }
    int sum = 0;
    for (auto& f : futs) sum += f.get();
    EXPECT_EQ(n.load(), 100);
    EXPECT_EQ(sum, 99 * 100 / 2);
}

WSS_TEST(Scheduler, FireAndForgetHandle) {
    wss::Scheduler pool(2);
    std::atomic<bool> done{false};
    auto h = pool.submit_task(
        std::make_shared<wss::FunctionTask>([&] { done.store(true); }));
    EXPECT_TRUE(h.valid());
    pool.wait_idle(std::chrono::seconds(5));
    EXPECT_TRUE(done.load());
    EXPECT_EQ(h.state(), wss::TaskState::Completed);
}

WSS_TEST(Scheduler, DoesNotLoseTasksOnShutdown) {
    constexpr int kTasks = 500;
    std::atomic<int> ran{0};
    std::vector<wss::TaskHandle> handles;
    {
        wss::Scheduler pool(4);
        for (int i = 0; i < kTasks; ++i) {
            handles.push_back(pool.submit_task(
                std::make_shared<wss::FunctionTask>(
                    [&] { ran.fetch_add(1, std::memory_order_relaxed); })));
        }
        pool.shutdown(true);
        for (auto& h : handles) {
            EXPECT_TRUE(h.state() == wss::TaskState::Completed);
        }
    }
    EXPECT_EQ(ran.load(), kTasks);
}

WSS_TEST(Scheduler, MetricsCountCompletions) {
    wss::Scheduler pool(2);
    auto metrics = std::make_shared<wss::MetricsCollector>();
    pool.set_observer(metrics);
    std::vector<wss::Future<int>> futs;
    for (int i = 0; i < 50; ++i) futs.push_back(pool.submit([i] { return i; }));
    for (auto& f : futs) f.get();
    pool.wait_idle(std::chrono::seconds(5));
    auto snap = pool.metrics();
    EXPECT_EQ(snap.finished, 50u);
    EXPECT_EQ(snap.failed, 0u);
}

WSS_TEST(Scheduler, MutexQueueBackendWorks) {
    wss::SchedulerConfig cfg;
    cfg.worker_count = 2;
    cfg.queue_kind = wss::QueueKind::Mutex;
    wss::Scheduler pool(cfg);
    auto f = pool.submit([] { return 7; });
    EXPECT_EQ(f.get(), 7);
}

WSS_TEST(Scheduler, RoundRobinVictimsWork) {
    wss::SchedulerConfig cfg;
    cfg.worker_count = 4;
    cfg.victim_policy = wss::VictimPolicyKind::RoundRobin;
    wss::Scheduler pool(cfg);
    std::atomic<int> n{0};
    std::vector<wss::Future<int>> futs;
    for (int i = 0; i < 200; ++i) {
        futs.push_back(pool.submit([&n] {
            n.fetch_add(1, std::memory_order_relaxed);
            return 1;
        }));
    }
    int sum = 0;
    for (auto& f : futs) sum += f.get();
    EXPECT_EQ(sum, 200);
    EXPECT_EQ(n.load(), 200);
}

WSS_TEST_MAIN()
