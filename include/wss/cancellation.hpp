#pragma once

#include <atomic>
#include <exception>
#include <memory>

namespace wss {

class TaskCancelled : public std::exception {
public:
    const char* what() const noexcept override { return "wss: task cancelled"; }
};

class ICancellationToken {
public:
    virtual ~ICancellationToken() = default;
    virtual bool is_cancelled() const noexcept = 0;
};

class CancellationTokenImpl final : public ICancellationToken {
public:
    CancellationTokenImpl() = default;

    bool is_cancelled() const noexcept override {
        return cancelled_.load(std::memory_order_acquire);
    }

    void cancel() noexcept { cancelled_.store(true, std::memory_order_release); }

private:
    std::atomic<bool> cancelled_{false};
};

class CancellationToken {
public:
    CancellationToken() = default;

    static CancellationToken none() { return CancellationToken{}; }

    explicit CancellationToken(std::shared_ptr<ICancellationToken> impl)
        : impl_(std::move(impl)) {}

    bool is_cancelled() const noexcept {
        return impl_ && impl_->is_cancelled();
    }

    bool valid() const noexcept { return static_cast<bool>(impl_); }

private:
    std::shared_ptr<ICancellationToken> impl_;
};

class CancellationSource {
public:
    CancellationSource()
        : impl_(std::make_shared<CancellationTokenImpl>()) {}

    CancellationToken token() const { return CancellationToken{impl_}; }

    void cancel() noexcept { impl_->cancel(); }

    bool is_cancelled() const noexcept { return impl_->is_cancelled(); }

private:
    std::shared_ptr<CancellationTokenImpl> impl_;
};

}  // namespace wss
