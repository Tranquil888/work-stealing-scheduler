#include <atomic>
#include <chrono>

#include <wss/scheduler.hpp>
#include <wss/verified.hpp>

#include "test_framework.hpp"

WSS_TEST(Verified, NoTaskLossOnCleanRun) {
    wss::SchedulerSnapshot snap;
    std::vector<std::uint64_t> submitted;
    for (std::uint64_t i = 1; i <= 10; ++i) {
        snap.tasks.push_back({i, wss::TaskState::Completed, 0});
        submitted.push_back(i);
    }
    EXPECT_TRUE(wss::check_no_task_loss(snap, submitted));
}

WSS_TEST(Verified, DetectsLoss) {
    wss::SchedulerSnapshot snap;
    snap.tasks.push_back({1, wss::TaskState::Completed, 0});
    EXPECT_FALSE(wss::check_no_task_loss(snap, {1, 2}));
}

WSS_TEST(Verified, DetectsDoubleRun) {
    std::vector<wss::TaskRecord> history = {
        {1, wss::TaskState::Pending, 0},
        {1, wss::TaskState::Running, 0},
        {1, wss::TaskState::Completed, 0},
        {1, wss::TaskState::Running, 1},
    };
    EXPECT_FALSE(wss::check_no_double_execution(history));
}

WSS_TEST(Verified, StealPreservesState) {
    EXPECT_TRUE(wss::check_steal_preserves_state(wss::TaskState::Pending,
                                                 wss::TaskState::Pending));
    EXPECT_FALSE(wss::check_steal_preserves_state(wss::TaskState::Pending,
                                                  wss::TaskState::Running));
}

WSS_TEST(Verified, LiveSchedulerLosesNothing) {
    wss::Scheduler pool(4);
    std::atomic<int> ran{0};
    std::vector<wss::TaskHandle> handles;
    for (int i = 0; i < 200; ++i) {
        handles.push_back(pool.submit_task(
            std::make_shared<wss::FunctionTask>(
                [&] { ran.fetch_add(1, std::memory_order_relaxed); })));
    }
    EXPECT_TRUE(pool.wait_idle(std::chrono::seconds(10)));
    wss::SchedulerSnapshot snap;
    std::vector<std::uint64_t> submitted;
    for (auto& h : handles) {
        snap.tasks.push_back({h.id(), h.state(), 0});
        submitted.push_back(h.id());
    }
    EXPECT_TRUE(wss::check_no_task_loss(snap, submitted));
    int completed = 0;
    for (auto& h : handles) {
        if (h.state() == wss::TaskState::Completed) ++completed;
    }
    EXPECT_EQ(completed, 200);
    EXPECT_EQ(ran.load(), 200);
}

WSS_TEST_MAIN()
