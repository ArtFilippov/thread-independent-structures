#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../../thread_pool/fine_grained_thread_pool.h"
#include "../../thread_pool/TaskManager.h"

#include <iostream>
#include <thread>
#include <chrono>

using namespace stepwise;

class test_shared_result : public ::testing::Test {
  public:
    void SetUp() { pool = std::make_unique<fine_grained_thread_pool>(1); }

    std::function<int()> m = []() -> int { return 1; };
    task_ptr<int> block{Task<int>::create(m, []() { return false; }, []() { return; })};

    std::shared_ptr<fine_grained_thread_pool> pool;

    TaskManager<int> tm{};
};

TEST_F(test_shared_result, test_add) {
    std::cout << "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" << std::endl;

    std::string ans = "stop";

    std::stringstream msg;
    auto task = [&, step = 0]() mutable -> std::optional<int> {
        msg << "step = " << step << " ";
        ++step;
        return {};
    };

    auto cond = [=]() -> bool { return false; };

    std::stringstream log;
    auto notice = [&log]() mutable -> void { log << "stop"; };

    {
        auto res = Task<int>::Result{};
        auto res1 = Task<int>::Result{};
        auto res2 = Task<int>::Result{};

        tm.template add(0, pool, task, cond, notice);
        // auto res1 = tm.add(1, pool, task, cond, std::move(notice));
        // auto res2 = tm.add(2, pool, task, cond, notice);
        // auto res3 = tm.add(3, pool, task, cond, notice);
        // auto res4 = tm.add(4, pool, task, cond, notice);
        // auto res5 = res4;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(log.str() == ans);
}

// TEST_F(test_shared_result, vector) {
//     constexpr int TASKS_NUMBER = 1000;

//     std::vector<stepwise::Task<int>::Result> results;

//     std::string ans = "stop";

//     std::stringstream msg;
//     auto task = [&, step = 0]() mutable -> std::optional<int> {
//         msg << "step = " << step << " ";
//         ++step;
//         return {};
//     };

//     auto cond = [=]() -> bool { return false; };

//     std::stringstream log;
//     auto notice = [=, &log]() mutable -> void { log << "stop"; };

//     {
//         auto res = block->share(pool, task, cond, notice);
//         for (int i = 0; i < TASKS_NUMBER; ++i) {
//             results.push_back(res);
//         }
//     }

//     results.clear();

//     std::this_thread::sleep_for(std::chrono::milliseconds(100));

//     ASSERT_TRUE(log.str() == ans);
// }
