#pragma once

#include "Task.h"

#include <tuple>
#include <map>

template <typename... ResultTyes> class TaskManager {
    std::tuple<std::map<int, ResultTyes>...> taskMap;

  public:
    template <typename Task, typename CancelCondition, typename OnComplete>
    auto add(int tid, std::shared_ptr<fine_grained_thread_pool> &pool, Task &&task, CancelCondition &&cancel_condition,
             OnComplete &&on_complete) {
        typedef typename std::result_of<Task()>::type result_type;

        auto &corrMap = std::get<result_type>(taskMap);

        auto task = corrMap.find(tid);

        if (task == corrMap.end()) {
            task = corMap.insert(std::pair(
                tid, Task<result_type>::create(std::move(task), std::move(cancel_condition), std::move(on_complete))));
        }

        return task->second->share(pool, std::move(task), std::move(cancel_condition), std::move(on_complete));
    }

    template <typename ResultType> void killTaskById<ResultType>(int tid) {
        auto &corrMap = std::get<result_type>(taskMap);

        auto task = corrMap.find(tid);

        if (task != corrMap.end()) {
            task->second->kill();
            corrMap->erase(task);
        }
    }
};
