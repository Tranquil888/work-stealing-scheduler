#include <atomic>
#include <thread>
#include <vector>

#include <wss/task.hpp>
#include <wss/work_queue.hpp>

#include "test_framework.hpp"

namespace {

wss::TaskPtr make_task() {
    return std::make_shared<wss::FunctionTask>([] {});
}

}  // namespace

WSS_TEST(MutexQueue, FifoOrder) {
    wss::MutexQueue q(false);
    auto a = make_task();
    auto b = make_task();
    EXPECT_TRUE(q.push(a));
    EXPECT_TRUE(q.push(b));
    wss::TaskPtr out;
    EXPECT_TRUE(q.try_pop(out));
    EXPECT_TRUE(out == a);
    EXPECT_TRUE(q.try_pop(out));
    EXPECT_TRUE(out == b);
    EXPECT_FALSE(q.try_pop(out));
}

WSS_TEST(MutexQueue, LifoOrder) {
    wss::MutexQueue q(true);
    auto a = make_task();
    auto b = make_task();
    q.push(a);
    q.push(b);
    wss::TaskPtr out;
    EXPECT_TRUE(q.try_pop(out));
    EXPECT_TRUE(out == b);
    EXPECT_TRUE(q.try_pop(out));
    EXPECT_TRUE(out == a);
}

WSS_TEST(MutexQueue, StealTakesFront) {
    wss::MutexQueue q(true);
    auto a = make_task();
    auto b = make_task();
    q.push(a);
    q.push(b);
    wss::TaskPtr out;
    EXPECT_TRUE(q.try_steal(out));
    EXPECT_TRUE(out == a);
}

WSS_TEST(ChaseLev, PushPopSingleThread) {
    wss::ChaseLevDeque q;
    std::vector<wss::TaskPtr> tasks;
    for (int i = 0; i < 100; ++i) tasks.push_back(make_task());
    for (auto& t : tasks) EXPECT_TRUE(q.push(t));
    EXPECT_EQ(q.size(), 100u);
    wss::TaskPtr out;
    for (int i = 99; i >= 0; --i) {
        EXPECT_TRUE(q.try_pop(out));
        EXPECT_TRUE(out == tasks[static_cast<std::size_t>(i)]);
    }
    EXPECT_TRUE(q.empty());
    EXPECT_FALSE(q.try_pop(out));
}

WSS_TEST(ChaseLev, StealIsFifo) {
    wss::ChaseLevDeque q;
    auto a = make_task();
    auto b = make_task();
    q.push(a);
    q.push(b);
    wss::TaskPtr out;
    EXPECT_TRUE(q.try_steal(out));
    EXPECT_TRUE(out == a);
    EXPECT_TRUE(q.try_steal(out));
    EXPECT_TRUE(out == b);
}

WSS_TEST(ChaseLev, ConcurrentPushStealLosesNothing) {
    wss::ChaseLevDeque q;
    constexpr int kTasks = 2000;
    std::atomic<int> popped{0};
    std::atomic<int> stolen{0};
    std::vector<wss::TaskPtr> tasks;
    for (int i = 0; i < kTasks; ++i) tasks.push_back(make_task());

    std::thread thief([&] {
        wss::TaskPtr out;
        while (popped + stolen < kTasks) {
            if (q.try_steal(out)) {
                ++stolen;
            } else {
                std::this_thread::yield();
            }
        }
    });
    for (auto& t : tasks) {
        while (!q.push(t)) std::this_thread::yield();
    }
    wss::TaskPtr out;
    while (popped + stolen < kTasks) {
        if (q.try_pop(out)) {
            ++popped;
        } else {
            std::this_thread::yield();
        }
    }
    thief.join();
    EXPECT_EQ(popped + stolen, kTasks);
    EXPECT_TRUE(q.empty());
}

WSS_TEST(QueueFactory, MakesBothKinds) {
    auto m = wss::make_queue(wss::QueueKind::Mutex);
    auto c = wss::make_queue(wss::QueueKind::ChaseLev);
    EXPECT_TRUE(m && c);
    EXPECT_TRUE(m->empty() && c->empty());
}

WSS_TEST_MAIN()
