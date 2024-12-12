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
        if (reference_count != nullptr) {
            reference_count->operator++();
        }
    }

    bool future_is_last() { return reference_count->load() < 0; }

  public:
    shared_result() : reference_count(new std::atomic_int(0)) {}

    shared_result(const shared_result<T> &other) noexcept
        : reference_count(other.reference_count), future(other.future) {
        if (reference_count != nullptr) {
            reference_count->operator++();
        }
    }

    shared_result(shared_result<T> &&other) noexcept
        : reference_count(other.reference_count), future(std::move(other.future)) {
        other.reference_count = nullptr;
    }

    ~shared_result() {
        if (reference_count != nullptr) {
            if (future_is_last()) {
                delete reference_count;
            } else {
                reference_count->operator--();
            }
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
        std::swap(reference_count, other.reference_count);
        std::swap(future, other.future);

        return *this;
    }

    const shared_result<T> &operator=(shared_result<T> &&other) {
        shared_result<T> tmp(std::move(other));

        std::swap(reference_count, tmp.reference_count);
        std::swap(future, tmp.future);

        return *this;
    }
};

template <typename T> class shared_result_control_block {
    std::atomic_bool is_valid{false};
    std::shared_future<T> future{};

    std::atomic_int *const reference_count{new std::atomic_int(0)};

  public:
    /**
     * @brief Проверяет, ждёт ли какой-либо поток завершения задачи
     * @return
     * - `true` задачу ожидают (есть связанный экземпляр `shared_result<T>`)
     *
     * - `false` задачу никто не ожидает
     */
    bool does_it_expect() { return reference_count->load() >= 0; }

    ~shared_result_control_block() {
        if (!does_it_expect()) {
            delete reference_count;
        } else {
            reference_count->operator--();
        }
    }

    /**
     * @brief Проверяет статус задачи
     * @return
     * - `true` - задача выполняется. Попытка привязать другую задачу (вызов `new_share()`) возбудит исключение
     *
     * - `false` - задача завершена. Попытка получить связанное значение (вызов `share()`) возбудит исключение
     */
    bool valid() { return is_valid.load(); }

    /**
     * @brief Сообщает о готовности задачи. Поместите Вызов этой функции в обработчик завершения задачи
     */
    void notify_about_readiness() { is_valid.store(false); }

    /**
     * @brief Получить значение связанное с существующей задачей. Задача могла быть добавлена в пул потоков или
     * поставлена с помощью `std::async`
     * @return `stepwise::shared_result<T>` - здесь появится значение, связанное с задачей в пуле потоков
     * @exception
     * - `std::logic_error` - если объект не связан ни с какой задачей (`valid()` возвращает `false`)
     */
    shared_result<T> share() {
        if (!valid()) {
            throw std::logic_error{
                "shared_result_control_block: attempt to share an invalid result. Use new_share() before"};
        }
        return shared_result<T>(reference_count, future);
    }

    /**
     * @brief Связать объект с существующей задачей. Задача могла быть добавлена в пул потоков или
     * поставлена с помощью `std::async`
     * @return `stepwise::shared_result<T>` - здесь появится значение, связанное с задачей в пуле потоков
     * @exception
     * - `std::logic_error` - если объект уже связан с другой задачей (`valid()` возвращает `true`)
     */
    shared_result<T> new_share(std::shared_future<T> f) {
        if (valid()) {
            throw std::logic_error{"shared_result_control_block: attempt to reset incomplete task. Wait for the task "
                                   "to complete or destroy all related shared_result"};
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
