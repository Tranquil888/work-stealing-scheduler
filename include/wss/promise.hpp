#pragma once

#include <exception>
#include <memory>
#include <utility>

#include <wss/future.hpp>
#include <wss/shared_state.hpp>

namespace wss {

template <typename T>
class Promise {
public:
    Promise() : state_(std::make_shared<SharedState<T>>()) {}
    Promise(const Promise&) = delete;
    Promise& operator=(const Promise&) = delete;
    Promise(Promise&&) noexcept = default;
    Promise& operator=(Promise&&) noexcept = default;

    ~Promise() {
        if (state_ && !state_->is_ready()) {
            state_->set_exception(std::make_exception_ptr(BrokenPromise{}));
        }
    }

    Future<T> get_future() const { return Future<T>{state_}; }

    bool valid() const noexcept { return static_cast<bool>(state_); }

    void set_value(T value) {
        if (!state_) throw TaskCancelled{};
        state_->set_value(std::move(value));
    }

    void set_exception(std::exception_ptr ex) {
        if (!state_) throw TaskCancelled{};
        state_->set_exception(std::move(ex));
    }

    void set_cancelled() {
        if (!state_) throw TaskCancelled{};
        state_->set_cancelled();
    }

private:
    std::shared_ptr<SharedState<T>> state_;
};

template <>
class Promise<void> {
public:
    Promise() : state_(std::make_shared<SharedState<void>>()) {}
    Promise(const Promise&) = delete;
    Promise& operator=(const Promise&) = delete;
    Promise(Promise&&) noexcept = default;
    Promise& operator=(Promise&&) noexcept = default;

    ~Promise() {
        if (state_ && !state_->is_ready()) {
            state_->set_exception(std::make_exception_ptr(BrokenPromise{}));
        }
    }

    Future<void> get_future() const { return Future<void>{state_}; }

    bool valid() const noexcept { return static_cast<bool>(state_); }

    void set_value() {
        if (!state_) throw TaskCancelled{};
        state_->set_value();
    }

    void set_exception(std::exception_ptr ex) {
        if (!state_) throw TaskCancelled{};
        state_->set_exception(std::move(ex));
    }

    void set_cancelled() {
        if (!state_) throw TaskCancelled{};
        state_->set_cancelled();
    }

private:
    std::shared_ptr<SharedState<void>> state_;
};

}  // namespace wss
