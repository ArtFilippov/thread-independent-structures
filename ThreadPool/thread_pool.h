#pragma once

#include "function_wrapper.h"
#include "join_threads.h"
#include "../SafeQueue/threadsafe_queue.h"

#include <atomic>
#include <future>

class thread_pool {
    std::atomic_bool isWorking{true};
    threadsafe_queue<function_wrapper> tasks;
    std::vector<std::thread> threads;
    join_threads joiner;

  private:
    void working_thread() {
        while (isWorking) {
            function_wrapper task;
            if (tasks.try_pop(task)) {
                task();
            } else {
                std::this_thread::yield();
            }
        }
    }

  public:
    thread_pool(unsigned number_of_threads = 0) : joiner(threads) {
        try {
            if (number_of_threads == 0) {
                number_of_threads = std::thread::hardware_concurrency();
            }

            for (unsigned i = 0; i < number_of_threads; ++i) {
                threads.push_back(std::thread(&thread_pool::working_thread, this));
            }
        } catch (...) {
            isWorking = false;
            throw;
        }
    }
    ~thread_pool() { isWorking = false; }

    template <typename Callable> auto submit(Callable f) {
        typedef typename std::result_of<Callable()>::type result_type;
        std::packaged_task<result_type()> task(std::move(f));
        std::future<result_type> result(task.get_future());
        tasks.push(std::move(task));
        return result;
    }
};
