#pragma once

#include <atomic>
#include <future>

namespace stepwise {

template <typename T> class shared_result_control_block;

template <typename T> class shared_result {
    friend class shared_result_control_block<T>;

    std::shared_future<T> future{};
    std::atomic_int *reference_count;

    shared_result(std::atomic_int *ref_count, const std::shared_future<T> future) noexcept
        : reference_count(ref_count), future(future) {
        reference_count->operator++();
    }

  public:
    shared_result(const shared_result<T> &other) noexcept
        : reference_count(other.reference_count), future(other.future) {
        reference_count->operator++();
    }

    ~shared_result() { reference_count->operator--(); }

    void wait() { future.wait(); }

    template <class Rep, class Period>
    std::future_status wait_for(const std::chrono::duration<Rep, Period> &timeout_duration) {
        return future.wait_for(timeout_duration);
    }

    template <class Clock, class Duration>
    std::future_status wait_until(const std::chrono::time_point<Clock, Duration> &timeout_time) {
        return future.wait_until(timeout_time);
    }

    bool try_get() { return (future.wait_for(0) == std::future_status::ready); }

    const T &get() { return future.get(); }

    const shared_result<T> &operator=(shared_result<T> other) {
        auto tmp = other.reference_count;
        other.reference_count = this->reference_count;
        this->reference_count = tmp;

        future = other.future;

        return *this;
    }

    const shared_result<T> &operator=(const shared_result<T> &&) = delete;
};

template <typename T> class shared_result_control_block {
    std::atomic_bool is_valid{false};
    std::shared_future<T> future{};

    std::atomic_int *const reference_count{new std::atomic_int(0)};

  public:
    ~shared_result_control_block() { delete reference_count; }

    bool valid() { return is_valid.load(); }

    void notify_about_readiness() { is_valid.store(false); }

    bool does_it_expect() { return reference_count->load() >= 0; }

    shared_result<T> share() { return shared_result<T>(reference_count, future); }

    shared_result<T> new_share(std::shared_future<T> f) {
        if (valid()) {
            throw std::logic_error{"shared_result_control_block: attempt to reset incomplete task"};
        }
        future = f;
        is_valid.store(true);

        auto res = shared_result<T>(reference_count, future);
        reference_count->store(0);
        return res;
    }

    const shared_result_control_block<T> &operator=(const shared_result_control_block<T> &) = delete;
    const shared_result_control_block<T> &operator=(const shared_result_control_block<T> &&) = delete;
};
} // namespace stepwise
