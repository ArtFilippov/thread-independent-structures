#pragma once

#include <atomic>
#include <future>
#include <mutex>
#include <memory>
#include <functional>
#include <type_traits>

#include <iostream>

#include "fine_grained_thread_pool.h"
#include "stepwise_function_wrapper.h"

namespace stepwise {

template <typename T> class Task : public std::enable_shared_from_this<Task<T>> {
  public:
    /**
     * @brief Класс для управления результатом задачи.
     * Позволяет ожидать завершения задачи и получать ее результат.
     *
     * @tparam T Тип результата задачи.
     */
    class Result {
        friend class Task;

      private:
        // Результат задачи
        std::shared_future<T> task_future;

        // Счетчик ссылок на результат задачи
        std::shared_ptr<std::atomic_int> result_reference_count;

        /**
         * @brief Конструктор. Создает Result, связанный с задачей.
         *
         * @param future Результат задачи.
         */
        Result(const std::shared_future<T> &future) noexcept
            : result_reference_count(std::make_shared<std::atomic_int>(1)), task_future(future) {}

      public:
        /**
         * @brief Конструктор по умолчанию. Создает пустой объект Result.
         */
        Result() : result_reference_count(nullptr) {}

        /**
         * @brief Конструктор копирования.
         */
        Result(const Result &other) noexcept
            : result_reference_count(other.result_reference_count), task_future(other.task_future) {
            if (result_reference_count) {
                result_reference_count->fetch_add(1);
            }
        }

        /**
         * @brief Конструктор перемещения.
         */
        Result(Result &&other) noexcept
            : result_reference_count(std::move(other.result_reference_count)),
              task_future(std::move(other.task_future)) {
            other.result_reference_count = nullptr;
        }

        /**
         * @brief Деструктор. Уменьшает счетчик ссылок.
         */
        ~Result() {
            if (result_reference_count) {
                result_reference_count->fetch_sub(1);
            }
        }

        /**
         * @brief Возвращает количество активных ссылок на результат задачи.
         */
        int count() const { return result_reference_count ? result_reference_count->load() : 1; }

        /**
         * @brief Ожидает завершения задачи.
         */
        void wait() const { task_future.wait(); }

        /**
         * @brief Ожидает завершения задачи с таймаутом.
         */
        template <class Rep, class Period>
        std::future_status wait_for(const std::chrono::duration<Rep, Period> &timeout_duration) const {
            return task_future.wait_for(timeout_duration);
        }

        /**
         * @brief Проверяет, готов ли результат задачи.
         */
        bool is_ready() const {
            return task_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
        }

        /**
         * @brief Возвращает результат задачи.
         */
        const T &get() const { return task_future.get(); }

        /**
         * @brief проверяет связан ли результат с какой-то задачей
         */
        bool empty() { return result_reference_count.get() == nullptr; }

        // Оператор присваивания
        Result &operator=(Result other) {
            std::swap(result_reference_count, other.result_reference_count);
            std::swap(task_future, other.task_future);
            return *this;
        }
    };

  private:
    // Указывает, активна ли задача (выполняется ли она в данный момент)
    std::atomic_bool is_task_active{false};

    std::atomic_bool kill_flag{false};

    std::mutex share_lock_mut;

    std::function<bool(void)> cancel_condition{[]() { return false; }};

    std::function<void(void)> on_complete{[]() { return; }};

    std::function<std::optional<T>(void)> main_func;

    Result result;

    // Приватный конструктор для создания объекта через фабричный метод

    Task(std::function<std::optional<T>(void)> main_func, std::function<bool(void)> cancel_condition,
         std::function<void(void)> on_complete)
        : main_func(main_func), cancel_condition(cancel_condition), on_complete(on_complete) {}

    auto wrap_service_function() {
        auto wrapped_cancel_cond = [=, con_cond = cancel_condition,
                                    control_block = this->shared_from_this()]() mutable -> bool {
            return con_cond() || !control_block->has_active_results() || control_block->need_to_kill();
        };

        auto wrapped_callback = [=, on_compl = on_complete,
                                 control_block = this->shared_from_this()]() mutable -> void {
            control_block->mark_task_as_complete();
            on_compl();
        };

        auto wrapped_task = [=, main_f = main_func]() mutable -> std::optional<T> { return main_f(); };

        return stepwise_function_wrapper::wrap(std::move(wrapped_task), std::move(wrapped_cancel_cond),
                                               std::move(wrapped_callback));
    }

  public:
    /**
     * @brief Фабричный метод для создания объекта Task.
     *
     * @return std::shared_ptr<Task<T>> Указатель на созданный объект.
     */
    static auto create(
        std::function<std::optional<T>(void)> main_func, std::function<bool(void)> cancel_condition,
        std::function<void(void)> on_complete = []() { return; }) {
        auto instance = std::shared_ptr<Task<T>>(new Task<T>(main_func, cancel_condition, on_complete));

        return instance;
    }

    static auto
    create(std::function<std::optional<T>(void)> main_func, std::function<void(void)> on_complete = []() { return; }) {
        auto instance = Task<T>::create(main_func, []() { return false; }, on_complete);

        return instance;
    }

    void kill() { kill_flag.store(true); }

    bool need_to_kill() { return kill_flag.load(); }

    /**
     * @brief Связывает задачу с пулом потоков. Если задача уже выполняется, возвращает существующий результат.
     *
     * @param pool Пул потоков, в котором будет выполняться задача.
     * @param task Задача, которая будет выполняться.
     * @param cancel_condition Условие досрочного завершения задачи.
     * @param on_complete Обработчик завершения задачи.
     * @return Result<T> Объект, представляющий результат задачи.
     */
    Result share(std::shared_ptr<fine_grained_thread_pool> &pool) {
        std::lock_guard<std::mutex> lg{share_lock_mut};

        bool current = false;

        Result resToRet;
        // Если задача не активна, инициализируем новую задачу
        if (is_task_active.compare_exchange_strong(current, true)) {
            auto task_base = wrap_service_function();

            kill_flag.store(false);

            result = Result(pool->submit(task_base).share()); // result_reference_count = 1;
            resToRet = result;                                // result_reference_count = 2;
            result.result_reference_count->store(1);          // result_reference_count = 1;
        } else {
            resToRet = result; // после завершения функции всего будет 1 копия снаружи и одна копия внутри
        }

        return resToRet;
    }

    Result share(std::shared_ptr<fine_grained_thread_pool> &pool, std::shared_ptr<Task<T>> other_ptr) {
        std::lock_guard<std::mutex> lg{share_lock_mut};

        Task<T> &other = *other_ptr;

        bool current = false;

        Result resToRet;

        // Если задача не активна, инициализируем новую задачу
        if (is_task_active.compare_exchange_strong(current, true)) {
            main_func = other.main_func;
            cancel_condition = other.cancel_condition;
            on_complete = other.on_complete;

            auto task_base = wrap_service_function();

            kill_flag.store(false);

            result = Result(pool->submit(task_base).share()); // result_reference_count = 1;
            resToRet = result;                                // result_reference_count = 2;
            result.result_reference_count->store(1);          // result_reference_count = 1;
        } else {
            resToRet = result; // после завершения функции всего будет 1 копия снаружи и одна копия внутри
        }

        return resToRet;
    }

    /**
     * @brief Проверяет, ожидает ли кто-либо результат задачи.
     *
     * @return true Если есть активные ссылки на результат задачи.
     * @return false Если никто не ожидает результат задачи.
     */
    bool has_active_results() { return result.count() > 0; }

    /**
     * @brief Уведомляет, что задача завершена.
     * Этот метод должен быть вызван в обработчике завершения задачи.
     */
    void mark_task_as_complete() { is_task_active.store(false); }

    Task(const Task<T> &) = delete;

    Task(Task<T> &&) = delete;

    const Task<T> &operator=(const Task<T> &) = delete;
    const Task<T> &operator=(Task<T> &&) = delete;
};

template <typename T> using task_ptr = std::shared_ptr<Task<T>>;

} // namespace stepwise
