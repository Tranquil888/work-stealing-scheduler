#pragma once

#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <wss/cancellation.hpp>
#include <wss/task.hpp>

namespace wss {

template <typename T>
class SharedState {
public:
    SharedState() = default;
    SharedState(const SharedState&) = delete;
    SharedState& operator=(const SharedState&) = delete;

    void set_value(T value) { fulfill(std::move(value)); }

    void set_exception(std::exception_ptr ex) {
        std::vector<std::function<void()>> run;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (ready_) return;
            storage_ = ex;
            ready_ = true;
            run.swap(continuations_);
        }
        cond_.notify_all();
        for (auto& cb : run) cb();
    }

    void set_cancelled() { set_exception(std::make_exception_ptr(TaskCancelled{})); }

    bool is_ready() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return ready_;
    }

    void wait() const {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] { return ready_; });
    }

    template <typename Rep, typename Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& d) const {
        std::unique_lock<std::mutex> lock(mutex_);
        return cond_.wait_for(lock, d, [this] { return ready_; });
    }

    T get() {
        wait();
        std::lock_guard<std::mutex> lock(mutex_);
        if (std::holds_alternative<T>(storage_)) {
            if constexpr (std::is_move_constructible_v<T>) {
                return std::move(std::get<T>(storage_));
            } else {
                return std::get<T>(storage_);
            }
        }
        std::rethrow_exception(std::get<std::exception_ptr>(storage_));
    }

    void add_continuation(std::function<void()> cb) {
        bool run_now = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (ready_) {
                run_now = true;
            } else {
                continuations_.push_back(std::move(cb));
            }
        }
        if (run_now) cb();
    }

private:
    void fulfill(T value) {
        std::vector<std::function<void()>> run;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (ready_) return;
            storage_ = std::move(value);
            ready_ = true;
            run.swap(continuations_);
        }
        cond_.notify_all();
        for (auto& cb : run) cb();
    }

    mutable std::mutex mutex_;
    mutable std::condition_variable cond_;
    std::variant<std::monostate, T, std::exception_ptr> storage_;
    bool ready_{false};
    std::vector<std::function<void()>> continuations_;
};

template <>
class SharedState<void> {
public:
    SharedState() = default;
    SharedState(const SharedState&) = delete;
    SharedState& operator=(const SharedState&) = delete;

    void set_value() { fulfill(); }

    void set_exception(std::exception_ptr ex) {
        std::vector<std::function<void()>> run;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (ready_) return;
            storage_ = ex;
            ready_ = true;
            run.swap(continuations_);
        }
        cond_.notify_all();
        for (auto& cb : run) cb();
    }

    void set_cancelled() { set_exception(std::make_exception_ptr(TaskCancelled{})); }

    bool is_ready() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return ready_;
    }

    void wait() const {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] { return ready_; });
    }

    template <typename Rep, typename Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& d) const {
        std::unique_lock<std::mutex> lock(mutex_);
        return cond_.wait_for(lock, d, [this] { return ready_; });
    }

    void get() {
        wait();
        std::lock_guard<std::mutex> lock(mutex_);
        if (std::holds_alternative<std::exception_ptr>(storage_)) {
            std::rethrow_exception(std::get<std::exception_ptr>(storage_));
        }
    }

    void add_continuation(std::function<void()> cb) {
        bool run_now = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (ready_) {
                run_now = true;
            } else {
                continuations_.push_back(std::move(cb));
            }
        }
        if (run_now) cb();
    }

private:
    void fulfill() {
        std::vector<std::function<void()>> run;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (ready_) return;
            storage_ = std::monostate{};
            ready_ = true;
            run.swap(continuations_);
        }
        cond_.notify_all();
        for (auto& cb : run) cb();
    }

    mutable std::mutex mutex_;
    mutable std::condition_variable cond_;
    std::variant<std::monostate, std::exception_ptr> storage_{std::monostate{}};
    bool ready_{false};
    std::vector<std::function<void()>> continuations_;
};

}  // namespace wss
