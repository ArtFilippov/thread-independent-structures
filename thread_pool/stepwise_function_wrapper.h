#pragma once

#include <memory>
#include <future>
#include <optional>
#include <exception>

namespace stepwise {
class bad_value : public std::exception {
    std::string msg;

  public:
    bad_value(const std::string &message) : msg(message) {}
    const char *what() const noexcept override { return msg.c_str(); }
};

class out_of_time : public std::exception {
    std::string msg;

  public:
    out_of_time(const std::string &message) : msg(message) {}
    const char *what() const noexcept override { return msg.c_str(); }
};
} // namespace stepwise

template <typename T> struct isOptional : std::false_type {};
template <typename T> struct isOptional<std::optional<T>> : std::true_type {};

class stepwise_function_wrapper;

template <typename T> struct wrapped_function {
    std::shared_ptr<stepwise_function_wrapper> function{nullptr};
    std::future<T> future{};

    wrapped_function() {}

    wrapped_function(std::shared_ptr<stepwise_function_wrapper> &function, std::future<T> &&future)
        : function(function), future(std::move(future)) {}

    wrapped_function(wrapped_function &&other) : function(other.function), future(std::move(future)) {}

    const wrapped_function &operator=(wrapped_function &&other) {
        std::swap(function, other.function);

        auto tmp = std::move(future);

        future = std::move(other.future);

        other.future = std::move(tmp);

        return *this;
    }
};

class stepwise_function_wrapper {
    struct impl_base {
        virtual void step() = 0;
        virtual bool is_done() = 0;
        virtual ~impl_base() = default;
    };

    std::unique_ptr<impl_base> impl;

    template <typename Cond, typename F, typename Notice> struct impl_type : impl_base {
        typedef typename std::result_of<F()>::type::value_type result_type;
        // класс является обёрткой над функцией, возвращающей std::optional. При этом если возвращается пустое значение,
        // std::promise не устанавливается. Если возвращается не пустое значение, в std::promise устанавливается
        // std::optional::value

        F f_;
        Cond c_;
        Notice n_;
        std::promise<result_type> promise;
        std::atomic_bool done{false};

        impl_type(std::promise<result_type> promise, Cond &&c, F &&f, Notice &&n)
            : f_(std::move(f)), c_(std::move(c)), n_(std::move(n)), promise(std::move(promise)) {}

        void step() override {
            try {
                std::optional<result_type> opt = f_();
                if (opt.has_value()) {
                    done = true;
                    n_();
                    promise.set_value(opt.value());
                }
            } catch (...) {
                done = true;
                n_();
                promise.set_exception(std::current_exception());
            }
        }
        bool is_done() override {
            if (done || c_()) {
                if (!done) {
                    n_();
                    promise.set_exception(
                        std::make_exception_ptr(stepwise::bad_value{"stepwise_function_wrapper: value is incomplete"}));
                }
                return true;
            }

            return false;
        };
    };

  public:
    template <typename Cond, typename F, typename Notice>
    stepwise_function_wrapper(std::promise<typename std::result_of<F()>::type::value_type> promise, Cond &&c, F &&f,
                              Notice &&n)
        : impl(new impl_type<Cond, F, Notice>(std::move(promise), std::move(c), std::move(f), std::move(n))) {}
    stepwise_function_wrapper() = default;
    stepwise_function_wrapper(stepwise_function_wrapper &) = delete;
    stepwise_function_wrapper(const stepwise_function_wrapper &) = delete;
    stepwise_function_wrapper(stepwise_function_wrapper &&other) : impl(std::move(other.impl)) {}
    ~stepwise_function_wrapper() {}

    void operator()() { impl->step(); };
    void step() { impl->step(); }
    bool is_done() { return impl->is_done(); }

    stepwise_function_wrapper &operator=(const stepwise_function_wrapper &) = delete;
    stepwise_function_wrapper &operator=(stepwise_function_wrapper &&other) {
        impl = std::move(other.impl);
        return *this;
    }

    template <typename Callable, typename BoolFunc, typename Notice>
    static auto wrap(Callable &&f, BoolFunc &&cond, Notice &&n) {
        typedef typename std::result_of<Callable()>::type result_type;
        if constexpr (isOptional<result_type>::value) {
            std::promise<typename result_type::value_type> promise;
            std::future<typename result_type::value_type> result(promise.get_future());
            auto task = std::make_shared<stepwise_function_wrapper>(
                std::move(promise), std::move(cond),
                std::move([func = std::move(f)]() mutable -> std::optional<typename result_type::value_type> {
                    return func();
                }),
                std::move(n));
            return wrapped_function<typename result_type::value_type>(task, std::move(result));
        } else {
            std::promise<result_type> promise;
            std::future<result_type> result(promise.get_future());
            auto task = std::make_shared<stepwise_function_wrapper>(
                std::move(promise), std::move(cond),
                std::move([f]() mutable -> std::optional<result_type> { return std::optional<result_type>{f()}; }),
                std::move(n));
            return wrapped_function<result_type>(task, std::move(result));
        }
    }
};
