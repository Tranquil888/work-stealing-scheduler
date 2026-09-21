#pragma once

#include <memory>

namespace wss {

class ISchedulingPolicy {
public:
    virtual ~ISchedulingPolicy() = default;
    virtual bool lifo() const noexcept = 0;
    virtual const char* name() const noexcept = 0;
};

class FifoPolicy final : public ISchedulingPolicy {
public:
    bool lifo() const noexcept override { return false; }
    const char* name() const noexcept override { return "fifo"; }
};

class LifoPolicy final : public ISchedulingPolicy {
public:
    bool lifo() const noexcept override { return true; }
    const char* name() const noexcept override { return "lifo"; }
};

}  // namespace wss
