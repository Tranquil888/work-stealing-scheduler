#pragma once

#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include <wss/cancellation.hpp>
#include <wss/shared_state.hpp>
#include <wss/task.hpp>

namespace wss {

template <typename T>
class Future;

template <typename T>
class Promise;

template <typename T>
class FutureTask final : public TaskBase {
public:
    using Func = std::function<T()>;

    FutureTask(Func func, std::shared_ptr<SharedState<T>> state,
               CancellationToken token = CancellationToken::none())
        : TaskBase(token, "future"), func_(std::move(func)), state_(std::move(state)) {}

    void execute() override {
        if (!begin_run()) {
            if (state_) state_->set_cancelled();
            return;
        }
        if (is_cancellation_requested()) {
            end_run(TaskState::Cancelled);
            if (state_) state_->set_cancelled();
            return;
        }
        try {
            if constexpr (std::is_void_v<T>) {
                func_();
                end_run(TaskState::Completed);
                if (state_) state_->set_value();
            } else {
                T value = func_();
                end_run(TaskState::Completed);
                if (state_) state_->set_value(std::move(value));
            }
        } catch (...) {
            end_run(TaskState::Failed);
            if (state_) state_->set_exception(std::current_exception());
        }
    }

private:
    Func func_;
    std::shared_ptr<SharedState<T>> state_;
};

template <typename T>
class Future {
public:
    Future() = default;
    explicit Future(std::shared_ptr<SharedState<T>> state) : state_(std::move(state)) {}

    bool valid() const noexcept { return static_cast<bool>(state_); }
    explicit operator bool() const noexcept { return valid(); }

    bool is_ready() const { return state_ && state_->is_ready(); }
    void wait() const { state_->wait(); }

    template <typename Rep, typename Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& d) const {
        return state_->wait_for(d);
    }

    T get() {
        if (!state_) throw TaskCancelled{};
        if constexpr (std::is_void_v<T>) {
            state_->get();
        } else {
            return state_->get();
        }
    }

    template <typename F, typename Executor>
    auto then(F&& func, Executor& exec) -> Future<std::invoke_result_t<F, T>> {
        using R = std::invoke_result_t<F, T>;
        auto next = std::make_shared<SharedState<R>>();
        Future<R> out(next);
        auto prev = state_;
        auto run = [prev, next, f = std::forward<F>(func), &exec]() mutable {
            try {
                if constexpr (std::is_void_v<T>) {
                    prev->get();
                    if constexpr (std::is_void_v<R>) {
                        f();
                        next->set_value();
                    } else {
                        next->set_value(f());
                    }
                } else {
                    T value = prev->get();
                    if constexpr (std::is_void_v<R>) {
                        f(std::move(value));
                        next->set_value();
                    } else {
                        next->set_value(f(std::move(value)));
                    }
                }
            } catch (...) {
                next->set_exception(std::current_exception());
            }
        };
        auto cont = [run = std::move(run), &exec]() mutable {
            exec.submit([run = std::move(run)]() mutable { run(); });
        };
        prev->add_continuation(std::move(cont));
        return out;
    }

    template <typename F, typename Executor>
    auto then_with_token(F&& func, Executor& exec, CancellationToken tok)
        -> Future<std::invoke_result_t<F, T>> {
        using R = std::invoke_result_t<F, T>;
        auto next = std::make_shared<SharedState<R>>();
        Future<R> out(next);
        auto prev = state_;
        auto run = [prev, next, f = std::forward<F>(func), &exec,
                    tok]() mutable {
            try {
                if (tok.is_cancelled()) {
                    next->set_cancelled();
                    return;
                }
                if constexpr (std::is_void_v<T>) {
                    prev->get();
                    if constexpr (std::is_void_v<R>) {
                        f();
                        next->set_value();
                    } else {
                        next->set_value(f());
                    }
                } else {
                    T value = prev->get();
                    if constexpr (std::is_void_v<R>) {
                        f(std::move(value));
                        next->set_value();
                    } else {
                        next->set_value(f(std::move(value)));
                    }
                }
            } catch (...) {
                next->set_exception(std::current_exception());
            }
        };
        auto cont = [run = std::move(run), &exec]() mutable {
            exec.submit([run = std::move(run)]() mutable { run(); });
        };
        prev->add_continuation(std::move(cont));
        return out;
    }

private:
    std::shared_ptr<SharedState<T>> state_;

    template <typename U>
    friend class Promise;
};

}  // namespace wss
