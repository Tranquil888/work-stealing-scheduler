#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

#include <wss/observer.hpp>
#include <wss/scheduler.hpp>

using namespace std::chrono_literals;

int main() {
    wss::Scheduler pool(4);
    pool.set_observer(std::make_shared<wss::MetricsCollector>());

    auto answer = pool.submit([] { return 42; });
    std::printf("answer = %d\n", answer.get());

    auto doubled = answer.then([](int v) { return v * 2; }, pool);
    std::printf("doubled = %d\n", doubled.get());

    wss::CancellationSource src;
    std::atomic<bool> ran{false};
    auto dropped = pool.submit_task(std::make_shared<wss::FunctionTask>(
        [&] { ran.store(true); }, src.token()));
    src.cancel();
    pool.wait_idle(5s);
    std::printf("cancelled task ran=%d state=%.*s\n", (int)ran.load(),
                (int)wss::to_string(dropped.state()).size(),
                wss::to_string(dropped.state()).data());

    pool.schedule_after(50ms, [] { std::printf("timer fired\n"); });
    auto every = pool.schedule_every(100ms, [] { std::printf("tick\n"); });
    std::this_thread::sleep_for(250ms);
    every.cancel();

    pool.shutdown(true);
    auto m = pool.metrics();
    std::printf("started=%llu finished=%llu stolen=%llu\n",
                (unsigned long long)m.started, (unsigned long long)m.finished,
                (unsigned long long)m.stolen);
    return 0;
}
