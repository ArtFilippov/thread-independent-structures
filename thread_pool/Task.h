#pragma once

#include <atomic>
#include <future>
#include <mutex>
#include <memory>

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
            : result_reference_count(std::make_shared<std::atomic_int>(0)), task_future(future) {}

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
        int count() const { return result_reference_count ? result_reference_count->load() : 0; }

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

    // Функция и результат
    wrapped_function<T> task_base;

    Result result;

    // Приватный конструктор для создания объекта через фабричный метод
    Task() {}

    template <typename MainFunc, typename CancelCondition, typename OnComplete>
    void wrap_service_function(MainFunc &&task, CancelCondition &&cancel_condition, OnComplete &&on_complete) {
        auto wrapped_cancel_cond = [=, control_block = this->shared_from_this()]() mutable {
            return cancel_condition() || !control_block->has_active_results() || control_block->need_to_kill();
        };

        auto wrapped_callback = [=, control_block = this->shared_from_this()]() mutable {
            control_block->mark_task_as_complete();
            on_complete();
        };

        task_base = stepwise_function_wrapper::wrap(std::move(task), std::move(wrapped_cancel_cond),
                                                    std::move(wrapped_callback));
    }

  public:
    /**
     * @brief Фабричный метод для создания объекта Task.
     *
     * @return std::shared_ptr<Task<T>> Указатель на созданный объект.
     */
    template <typename MainFunc, typename CancelCondition, typename OnComplete>
    static auto create(MainFunc &&task, CancelCondition &&cancel_condition, OnComplete &&on_complete) {
        auto instance = std::shared_ptr<Task<T>>(new Task<T>());

        instance->wrap_service_function(std::move(task), std::move(cancel_condition), std::move(on_complete));
        instance->result = Result(instance->task_base.future.share());

        return instance;
    }

    template <typename MainFunc, typename CancelCondition>
    static std::shared_ptr<Task<T>> create(MainFunc &&task, CancelCondition &&cancel_condition) {
        return create(std::move(task), std::move(cancel_condition), []() { return; });
    }

    template <typename MainFunc> static std::shared_ptr<Task<T>> create(MainFunc &&task) {
        return create(std::move(task), []() -> bool { return false; }, []() { return; });
    }

    static std::shared_ptr<Task<T>> create() {
        return create([]() { return T{}; }, []() -> bool { return false; }, []() { return; });
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
    template <typename MainFunc, typename CancelCondition, typename OnComplete>
    Result share(std::shared_ptr<fine_grained_thread_pool> &pool, MainFunc &&task, CancelCondition &&cancel_condition,
                 OnComplete &&on_complete) {
        std::lock_guard<std::mutex> lg{share_lock_mut};

        bool current = false;

        // Если задача не активна, инициализируем новую задачу
        if (is_task_active.compare_exchange_strong(current, true)) {
            wrap_service_function(std::move(task), std::move(cancel_condition), std::move(on_complete));
            result = Result(pool->submit(task_base).share());
        }

        return result;
    }

    template <typename MainFunc, typename CancelCondition>
    Result share(std::shared_ptr<fine_grained_thread_pool> &pool, MainFunc &&task, CancelCondition &&cancel_condition) {
        return share(pool, std::move(task), std::move(cancel_condition), []() { return; });
    }

    template <typename MainFunc> Result share(std::shared_ptr<fine_grained_thread_pool> &pool, MainFunc &&task) {
        return share(pool, std::move(task), []() { return false; }, []() { return; });
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
