#pragma once

#include "stepwise_function_wrapper.h"
#include "join_threads.h"
#include "../safe_queue/threadsafe_queue.h"

#include <atomic>
#include <future>
#include <optional>

template <typename T> struct isOptional : std::false_type {};
template <typename T> struct isOptional<std::optional<T>> : std::true_type {};

class fine_grained_thread_pool {
    std::atomic_bool isWorking{true};
    threadsafe_queue<stepwise_function_wrapper> tasks;
    std::vector<std::thread> threads;
    join_threads joiner;

  private:
    void working_thread() {
        while (isWorking) {
            if (auto task = tasks.try_pop()) {
                task->step();
                if (!task->is_done()) {
                    tasks.push(task);
                }
            } else {
                std::this_thread::yield();
            }
        }
    }

  public:
    fine_grained_thread_pool(unsigned number_of_threads = 0) : joiner(threads) {
        try {
            if (number_of_threads == 0) {
                number_of_threads = std::thread::hardware_concurrency();
            }

            for (unsigned i = 0; i < number_of_threads; ++i) {
                threads.push_back(std::thread(&fine_grained_thread_pool::working_thread, this));
            }
        } catch (...) {
            isWorking = false;
            throw;
        }
    }
    ~fine_grained_thread_pool() { isWorking = false; }

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

    template <typename BoolFunc, typename Callable> auto submit(Callable &&f, BoolFunc &&cond) {
        return submit(f, cond, []() { return; });
    }

    template <typename Callable> auto submit(Callable &&f) {
        return submit(f, []() { return false; });
    }
};
