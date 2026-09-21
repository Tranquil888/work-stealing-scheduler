#include <wss/work_stealing_policy.hpp>

namespace wss {

thread_local std::mt19937 RandomVictimPolicy::rng_{std::random_device{}()};

RandomVictimPolicy::RandomVictimPolicy() = default;

std::size_t RandomVictimPolicy::pick_victim(std::size_t self, std::size_t count,
                                            int /*attempt*/) {
    if (count < 2) return self;
    std::uniform_int_distribution<std::size_t> dist(0, count - 2);
    std::size_t v = dist(rng_);
    return v >= self ? v + 1 : v;
}

RoundRobinVictimPolicy::RoundRobinVictimPolicy() = default;

std::size_t RoundRobinVictimPolicy::pick_victim(std::size_t self,
                                                std::size_t count,
                                                int /*attempt*/) {
    if (count < 2) return self;
    std::size_t v = next_.fetch_add(1, std::memory_order_relaxed) % (count - 1);
    return v >= self ? v + 1 : v;
}

std::unique_ptr<IWorkStealingPolicy> make_victim_policy(VictimPolicyKind kind) {
    if (kind == VictimPolicyKind::RoundRobin) {
        return std::make_unique<RoundRobinVictimPolicy>();
    }
    return std::make_unique<RandomVictimPolicy>();
}

}  // namespace wss
