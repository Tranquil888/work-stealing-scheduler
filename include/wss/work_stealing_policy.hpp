#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <random>

namespace wss {

class IWorkStealingPolicy {
public:
    virtual ~IWorkStealingPolicy() = default;
    virtual std::size_t pick_victim(std::size_t self, std::size_t count,
                                    int attempt) = 0;
    virtual const char* name() const noexcept = 0;
};

class RandomVictimPolicy final : public IWorkStealingPolicy {
public:
    RandomVictimPolicy();

    std::size_t pick_victim(std::size_t self, std::size_t count,
                            int attempt) override;
    const char* name() const noexcept override { return "random"; }

private:
    static thread_local std::mt19937 rng_;
};

class RoundRobinVictimPolicy final : public IWorkStealingPolicy {
public:
    RoundRobinVictimPolicy();

    std::size_t pick_victim(std::size_t self, std::size_t count,
                            int attempt) override;
    const char* name() const noexcept override { return "round_robin"; }

private:
    std::atomic<std::size_t> next_{0};
};

enum class VictimPolicyKind { Random, RoundRobin };

std::unique_ptr<IWorkStealingPolicy> make_victim_policy(VictimPolicyKind kind);

}  // namespace wss
