#include <chrono>
#include <stdexcept>
#include <string>

#include <wss/future.hpp>
#include <wss/promise.hpp>
#include <wss/scheduler.hpp>

#include "test_framework.hpp"

WSS_TEST(Future, SubmitReturnsValue) {
    wss::Scheduler pool(2);
    auto f = pool.submit([] { return 42; });
    EXPECT_EQ(f.get(), 42);
}

WSS_TEST(Future, VoidFuture) {
    wss::Scheduler pool(2);
    bool ran = false;
    auto f = pool.submit([&] { ran = true; });
    f.get();
    EXPECT_TRUE(ran);
}

WSS_TEST(Future, ExceptionPropagates) {
    wss::Scheduler pool(2);
    auto f = pool.submit([]() -> int { throw std::runtime_error("bad"); });
    bool caught = false;
    try {
        f.get();
    } catch (const std::runtime_error& e) {
        caught = std::string(e.what()) == "bad";
    }
    EXPECT_TRUE(caught);
}

WSS_TEST(Future, ThenChains) {
    wss::Scheduler pool(4);
    auto f = pool.submit([] { return 21; });
    auto g = f.then([](int v) { return v * 2; }, pool);
    EXPECT_EQ(g.get(), 42);
}

WSS_TEST(Future, ThenPropagatesFailure) {
    wss::Scheduler pool(4);
    auto f = pool.submit([]() -> int { throw std::runtime_error("up"); });
    auto g = f.then([](int v) { return v + 1; }, pool);
    bool caught = false;
    try {
        g.get();
    } catch (const std::runtime_error&) {
        caught = true;
    }
    EXPECT_TRUE(caught);
}

WSS_TEST(Future, PromiseDelivers) {
    wss::Promise<std::string> p;
    auto f = p.get_future();
    p.set_value("hello");
    EXPECT_TRUE(f.get() == "hello");
}

WSS_TEST(Future, BrokenPromiseThrows) {
    wss::Promise<int> p;
    auto f = p.get_future();
    p.set_exception(std::make_exception_ptr(std::runtime_error("gone")));
    bool caught = false;
    try {
        f.get();
    } catch (const std::runtime_error&) {
        caught = true;
    }
    EXPECT_TRUE(caught);
}

WSS_TEST(Future, WaitForTimesOut) {
    wss::Promise<int> p;
    auto f = p.get_future();
    EXPECT_FALSE(f.wait_for(std::chrono::milliseconds(10)));
    p.set_value(1);
    EXPECT_TRUE(f.wait_for(std::chrono::seconds(5)));
    EXPECT_EQ(f.get(), 1);
}

WSS_TEST_MAIN()
