#include <atomic>
#include <chrono>
#include <thread>

#include <wss/scheduler.hpp>

#include "test_framework.hpp"

WSS_TEST(Timer, FiresAfterDelay) {
    wss::Scheduler pool(2);
    std::atomic<bool> fired{false};
    pool.schedule_after(std::chrono::milliseconds(30),
                        [&] { fired.store(true); });
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!fired.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_TRUE(fired.load());
}

WSS_TEST(Timer, PeriodicFiresRepeatedly) {
    wss::Scheduler pool(2);
    std::atomic<int> n{0};
    auto h = pool.schedule_every(std::chrono::milliseconds(20),
                                 [&] { n.fetch_add(1); });
    EXPECT_TRUE(h.valid());
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    h.cancel();
    int got = n.load();
    EXPECT_TRUE(got >= 3);
    pool.wait_idle(std::chrono::seconds(5));
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    EXPECT_TRUE(n.load() <= got + 1);
}

WSS_TEST(Timer, CancelBeforeFireNeverRuns) {
    wss::Scheduler pool(2);
    std::atomic<bool> fired{false};
    auto h = pool.schedule_after(std::chrono::milliseconds(200),
                                 [&] { fired.store(true); });
    EXPECT_TRUE(h.cancel());
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_FALSE(fired.load());
}

WSS_TEST_MAIN()
