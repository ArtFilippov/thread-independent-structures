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
};
