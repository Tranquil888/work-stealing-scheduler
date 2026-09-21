#include <chrono>
#include <cstdio>
#include <vector>

#include <wss/scheduler.hpp>

namespace {

using Clock = std::chrono::steady_clock;

double run_case(wss::QueueKind kind, std::size_t workers, int tasks,
                int spin) {
    wss::SchedulerConfig cfg;
    cfg.worker_count = workers;
    cfg.queue_kind = kind;
    wss::Scheduler pool(cfg);
    auto start = Clock::now();
    std::vector<wss::Future<int>> futs;
    futs.reserve(static_cast<std::size_t>(tasks));
    for (int i = 0; i < tasks; ++i) {
        futs.push_back(pool.submit([spin] {
            volatile int x = 0;
            for (int j = 0; j < spin; ++j) x += j;
            return x;
        }));
    }
    for (auto& f : futs) f.get();
    auto elapsed = Clock::now() - start;
    double secs = std::chrono::duration<double>(elapsed).count();
    return tasks / secs;
}

const char* kind_name(wss::QueueKind k) {
    return k == wss::QueueKind::Mutex ? "mutex " : "chaselv";
}

}  // namespace

int main() {
    const std::size_t widths[] = {1, 2, 4, 8};
    std::printf("%-8s %-7s %12s %12s\n", "queue", "workers", "tiny/s", "medium/s");
    for (auto kind : {wss::QueueKind::Mutex, wss::QueueKind::ChaseLev}) {
        for (auto w : widths) {
            double tiny = run_case(kind, w, 50000, 0);
            double medium = run_case(kind, w, 5000, 2000);
            std::printf("%-8s %-7llu %12.0f %12.0f\n", kind_name(kind),
                        (unsigned long long)w, tiny, medium);
        }
    }
    return 0;
}
