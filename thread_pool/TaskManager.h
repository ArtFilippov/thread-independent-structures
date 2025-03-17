#pragma once

#include "Task.h"

#include <tuple>
#include <map>

template <typename ResultTyes> class TaskManager {
    std::tuple<std::map<int, Task<ResultTyes>>> taskMap;
    std::map<int, Task<ResultTyes>> taskMapSingle;

  public:
    template <typename MainFunc, typename CancelCondition, typename OnComplete>
    auto add(int tid, std::shared_ptr<fine_grained_thread_pool> &pool, MainFunc &&task,
             CancelCondition &&cancel_condition, OnComplete &&on_complete) {

        typedef typename std::result_of<MainFunc()>::type result_type;
        if constexpr (isOptional<result_type>::value) {
            std::map<int, Task<typename result_type::value_type>> &corrMap = taskMapSingle;
            auto taskIt = corrMap.find(tid);

            if (taskIt == corrMap.end()) {
                taskIt = corrMap.insert(std::pair(tid, Task<typename result_type::value_type>::create())).first;
            }
            return taskIt->share(pool, std::move(task), std::move(cancel_condition), std::move(on_complete));
        } else {
            auto &corrMap = std::get<std::map<int, Task<result_type>>>(taskMap);
            auto taskIt = corrMap.find(tid);

            if (taskIt == corrMap.end()) {
                taskIt = corrMap.insert(std::pair(tid, Task<result_type>::create())).first;
            }
            return taskIt->share(pool, std::move(task), std::move(cancel_condition), std::move(on_complete));
        }
    }

    template <typename ResultType> void killTaskById(int tid) {
        auto &corrMap = std::get<ResultType>(taskMap);

        auto task = corrMap.find(tid);

        if (task != corrMap.end()) {
            task->second->kill();
            corrMap.erase(task);
        }
    }
};
