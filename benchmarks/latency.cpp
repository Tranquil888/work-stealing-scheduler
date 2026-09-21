#include <algorithm>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <numeric>
#include <vector>

#include <wss/scheduler.hpp>

namespace {

using Clock = std::chrono::steady_clock;

double percentile(std::vector<double>& sorted_us, double p) {
    if (sorted_us.empty()) return 0.0;
    std::size_t i =
        std::min(sorted_us.size() - 1, static_cast<std::size_t>(
                                           p * sorted_us.size()));
    return sorted_us[i];
}

}  // namespace

int main() {
    wss::Scheduler pool(4);
    constexpr int kTasks = 20000;
    std::vector<double> lat_us;
    lat_us.reserve(kTasks);
    std::mutex lat_mutex;
    std::vector<wss::Future<void>> done;
    done.reserve(kTasks);
    for (int burst = 0; burst < 4; ++burst) {
        for (int i = 0; i < kTasks / 4; ++i) {
            auto submitted = Clock::now();
            done.push_back(pool.submit([submitted, &lat_us, &lat_mutex] {
                auto finished = Clock::now();
                double us =
                    std::chrono::duration<double, std::micro>(finished - submitted)
                        .count();
                std::lock_guard<std::mutex> lock(lat_mutex);
                lat_us.push_back(us);
            }));
        }
        for (auto& f : done) f.wait();
        done.clear();
    }
    std::sort(lat_us.begin(), lat_us.end());
    double mean =
        std::accumulate(lat_us.begin(), lat_us.end(), 0.0) / lat_us.size();
    std::printf("n=%llu mean=%.1fus p50=%.1fus p99=%.1fus max=%.1fus\n",
                (unsigned long long)lat_us.size(), mean,
                percentile(lat_us, 0.50), percentile(lat_us, 0.99),
                lat_us.back());
    return 0;
}
