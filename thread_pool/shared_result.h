#pragma once

#include <atomic>
#include <future>
#include <mutex>
#include <memory>

#include "fine_grained_thread_pool.h"

namespace stepwise {
template <typename T> class shared_result;

template <typename T> class shared_task : public std::enable_shared_from_this<shared_task<T>> {
    friend class shared_result<T>;

    std::atomic_bool is_valid{false};

    std::mutex result_mutex;
    std::shared_future<T> future{};
    std::shared_ptr<std::atomic_int> reference_count{new std::atomic_int{-1}};

    shared_task() {}

  public:
    static std::shared_ptr<shared_task<T>> make_task() { return std::shared_ptr<shared_task<T>>(new shared_task<T>()); }

    /**
     * @brief Проверяет, ждёт ли какой-либо поток завершения задачи
     * @return
     * - `true` задачу ожидают (есть связанный экземпляр `shared_result<T>`)
     *
     * - `false` задачу никто не ожидает
     */
    bool does_it_expect() { return reference_count->load() >= 0; }

    /**
     * @brief Сообщает о готовности задачи. Поместите Вызов этой функции в обработчик завершения задачи
     */
    void notify_about_readiness() { is_valid.store(false); }

    /**
     * @brief Связать объект с задачей. Если текущая задача завершена, ставится новая задача, иначе возвращается
     * `shared_result<T>` текущей задачи
     * @param pool Ссылка на пул потоков, в котором будет выполнятся задача
     * @param f Вызываемый объект, возвращающий `std::optional<возвращаемый тип>` (либо просто `возвращаемый тип`,
     * если задача рассчитана на один подход)  - задача для пула потоков
     * @param c Вызываемый объект, возвращающий `bool` - условие досрочного завершения задачи, например `[=]() {return
     * !shared_task->does_it_expect();}`
     * @param n Вызываемый объект, обработчик завершения задачи, например `[=]() {return
     * shared_task->notify_about_readiness();}`
     * @return `stepwise::shared_result<T>` - здесь появится значение, связанное с задачей в пуле потоков
     */
    template <typename F, typename Cond, typename Notice>
    shared_result<T> share(std::shared_ptr<fine_grained_thread_pool> pool, F f, Cond c, Notice n) {
        bool current = false;

        std::lock_guard<std::mutex> lg{result_mutex};
        if (is_valid.compare_exchange_strong(current, true)) {
            reference_count = std::make_shared<std::atomic_int>(-1);

            future = pool->submit(std::move(f), std::move(c), std::move(n)).share();

            auto res = shared_result<T>(reference_count, future);

            return res;
        } else {
            return shared_result<T>(reference_count, future);
        }
    }

    const shared_task<T> &operator=(const shared_task<T> &) = delete;
    const shared_task<T> &operator=(const shared_task<T> &&) = delete;
};

template <typename T> class shared_result {
    friend class shared_task<T>;

    std::shared_future<T> future{};
    std::shared_ptr<std::atomic_int> reference_count;

    shared_result(std::shared_ptr<std::atomic_int> ref_count, const std::shared_future<T> future) noexcept
        : reference_count(ref_count), future(future) {
        reference_count->fetch_add(1);
    }

  public:
    shared_result() : reference_count(nullptr) {}

    shared_result(const shared_result<T> &other) noexcept
        : reference_count(other.reference_count), future(other.future) {
        if (reference_count) {
            reference_count->fetch_add(1);
        }
    }

    shared_result(shared_result<T> &&other) noexcept
        : reference_count(other.reference_count), future(std::move(other.future)) {
        other.reference_count = nullptr;
    }

    ~shared_result() {
        if (reference_count) {
            reference_count->fetch_sub(1);
        }
    }

    void wait() { future.wait(); }

    template <class Rep, class Period>
    std::future_status wait_for(const std::chrono::duration<Rep, Period> &timeout_duration) {
        return future.wait_for(timeout_duration);
    }

    template <class Clock, class Duration>
    std::future_status wait_until(const std::chrono::time_point<Clock, Duration> &timeout_time) {
        return future.wait_until(timeout_time);
    }

    /**
     * @brief Проверяет, готов ли результат. Возвращается немедленно
     * @return
     * - `true` - результат готов
     *
     * - `false` - результат не готов
     */
    bool try_get() { return (future.wait_for(0) == std::future_status::ready); }

    const T &get() { return future.get(); }

    const shared_result<T> &operator=(shared_result<T> other) {
        reference_count.swap(other.reference_count);
        std::swap(future, other.future);

        return *this;
    }

    const shared_result<T> &operator=(shared_result<T> &&other) {
        shared_result<T> tmp(std::move(other));

        reference_count.swap(other.reference_count);
        std::swap(future, tmp.future);

        return *this;
    }
};

template <typename T> using task_ptr = std::shared_ptr<shared_task<T>>;

} // namespace stepwise
