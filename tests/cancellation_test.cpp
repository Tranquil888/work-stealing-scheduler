#include <atomic>
#include <chrono>
#include <thread>

#include <wss/cancellation.hpp>
#include <wss/scheduler.hpp>

#include "test_framework.hpp"

WSS_TEST(Cancellation, TokenStartsClear) {
    wss::CancellationSource src;
    EXPECT_FALSE(src.is_cancelled());
    EXPECT_FALSE(src.token().is_cancelled());
}

WSS_TEST(Cancellation, CancelSetsToken) {
    wss::CancellationSource src;
    auto tok = src.token();
    src.cancel();
    EXPECT_TRUE(src.is_cancelled());
    EXPECT_TRUE(tok.is_cancelled());
}

WSS_TEST(Cancellation, CancelIsMonotonic) {
    wss::CancellationSource src;
    src.cancel();
    src.cancel();
    EXPECT_TRUE(src.token().is_cancelled());
}

WSS_TEST(Cancellation, CancelledBeforeStartNeverRuns) {
    wss::Scheduler pool(2);
    wss::CancellationSource src;
    src.cancel();
    std::atomic<bool> ran{false};
    auto h = pool.submit_task(std::make_shared<wss::FunctionTask>(
        [&] { ran.store(true); }, src.token()));
    pool.wait_idle(std::chrono::seconds(5));
    EXPECT_FALSE(ran.load());
    EXPECT_EQ(h.state(), wss::TaskState::Cancelled);
}

WSS_TEST(Cancellation, HandleCancelStopsPendingTask) {
    wss::Scheduler pool(1);
    pool.submit_task(std::make_shared<wss::FunctionTask>(
        [] { std::this_thread::sleep_for(std::chrono::milliseconds(100)); }));
    std::atomic<bool> ran{false};
    auto h = pool.submit_task(std::make_shared<wss::FunctionTask>(
        [&] { ran.store(true); }));
    EXPECT_TRUE(h.cancel());
    pool.wait_idle(std::chrono::seconds(5));
    EXPECT_FALSE(ran.load());
    EXPECT_EQ(h.state(), wss::TaskState::Cancelled);
}

WSS_TEST(Cancellation, RunningTaskObservesToken) {
    wss::Scheduler pool(2);
    wss::CancellationSource src;
    std::atomic<bool> started{false};
    auto f = pool.submit_with_token(
        [tok = src.token(), &started] {
            started.store(true);
            for (int i = 0; i < 10000000; ++i) {
                if (tok.is_cancelled()) return -1;
                if ((i & 1023) == 0) std::this_thread::yield();
            }
            return 0;
        },
        src.token());
    while (!started.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    src.cancel();
    EXPECT_EQ(f.get(), -1);
}

WSS_TEST_MAIN()
