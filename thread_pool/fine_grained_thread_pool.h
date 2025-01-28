#pragma once

#include "stepwise_function_wrapper.h"
#include "../safe_queue/threadsafe_queue.h"

#include <atomic>
#include <future>
#include <optional>

template <typename T> struct isOptional : std::false_type {};
template <typename T> struct isOptional<std::optional<T>> : std::true_type {};

class fine_grained_thread_pool {

    class join_threads {
        std::vector<std::thread> threads_{};

      public:
        join_threads() {}

        ~join_threads() {
            for (int i = 0; i < (int) threads_.size(); ++i) {
                if (threads_[i].joinable()) {
                    threads_[i].join();
                }
            }
        }

        std::vector<std::thread> *operator->() { return &threads_; }
        // объект joiner должен разрушаться строго после разрушения вектора потоков. Поэтому решено поместить вектор
        // потоков в joiner, для снижения требований к клиентскому коду
    };

    std::atomic_bool isWorking{true};
    threadsafe_queue<stepwise_function_wrapper> tasks;
    join_threads joiner{};

  private:
    void working_thread() {
        while (isWorking) {
            auto task = tasks.wait_and_pop();
            task->step();
            if (!task->is_done()) {
                tasks.push(task);
            }
        }
    }

  public:
    fine_grained_thread_pool(unsigned number_of_threads = 0) {
        try {
            if (number_of_threads == 0) {
                number_of_threads = std::thread::hardware_concurrency();
            }

            for (unsigned i = 0; i < number_of_threads; ++i) {
                joiner->push_back(std::thread(&fine_grained_thread_pool::working_thread, this));
            }
        } catch (...) {
            isWorking = false;
            throw;
        }
    }
    ~fine_grained_thread_pool() { isWorking = false; }

    /**
     * @brief
     * - Помещает вызываемый объект `f` в очередь. Когда очередь дойдёт до `f`, один поток из пула примется за
     * выполнение `f()`
     *
     * - Если при вызове `f()` вернётся пустое значение, задача перемещается в конец очереди, позже один из потоков
     * вернётся к её выполнению
     *
     * - Если при вызове `f()` вернётся не пустое значение, задача считается выполненной
     *
     * - После каждого подхода к задаче, проверяется условие `cond()`. Если оно `true`, задача завершается досрочно
     *
     * - Когда задача завершена, вне зависимости досрочно или нет, вызывается `n()`
     * @param f Вызываемый объект, возвращающий `std::optional<возвращаемый тип>` (либо просто `возвращаемый тип`, если
     * задача рассчитана на один подход)  - задача для пула потоков
     * @param cond Вызываемый объект, возвращающий `bool` - условие досрочного завершения задачи
     * @param n Вызываемый объект, обработчик завершения задачи
     * @return Объект `std::future<возвращаемый тип>`, связанный с задачей `f`. Когда задача будет выполнена, в этом
     * объекте появится результат её выполнения
     */
    template <typename BoolFunc, typename Callable, typename Notice>
    auto submit(Callable &&f, BoolFunc &&cond, Notice &&n) {
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
            tasks.push(task);
            return result;
        } else {
            std::promise<result_type> promise;
            std::future<result_type> result(promise.get_future());
            auto task = std::make_shared<stepwise_function_wrapper>(
                std::move(promise), std::move(cond),
                std::move([f]() mutable -> std::optional<result_type> { return std::optional<result_type>{f()}; }),
                std::move(n));
            tasks.push(task);
            return result;
        }
    }

    /**
     * @brief
     * - Помещает вызываемый объект `f` в очередь. Когда очередь дойдёт до `f`, один поток из пула примется за
     * выполнение `f()`
     *
     * - Если при вызове `f()` вернётся пустое значение, задача перемещается в конец очереди, позже один из потоков
     * вернётся к её выполнению
     *
     * - Если при вызове `f()` вернётся не пустое значение, задача считается выполненной
     *
     * - После каждого подхода к задаче, проверяется условие `cond()`. Если оно `true`, задача завершается досрочно
     * @param f Вызываемый объект, возвращающий `std::optional<возвращаемый тип>` (либо просто `возвращаемый тип`, если
     * задача рассчитана на один подход) - задача для пула потоков
     * @param cond Вызываемый объект, возвращающий `bool` - условие досрочного завершения задачи
     * @return Объект `std::future<возвращаемый тип>`, связанный с задачей `f`. Когда задача будет выполнена, в этом
     * объекте появится результат её выполнения
     */
    template <typename BoolFunc, typename Callable> auto submit(Callable &&f, BoolFunc &&cond) {
        return submit(f, cond, []() { return; });
    }

    /**
     * @brief Помещает вызываемый объект `f` в очередь. Когда очередь дойдёт до `f`, один поток из пула примется за
     * выполнение `f()` до её завершения
     * @param f Вызываемый объект, задача для пула потоков
     * @return Объект `std::future`, связанный с задачей `f`. Когда задача будет выполнена, в этом объекте появится
     * результат её выполнения
     */
    template <typename Callable> auto submit(Callable &&f) {
        return submit(f, []() { return false; });
    }
};
