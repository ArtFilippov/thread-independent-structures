#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../../thread_pool/fine_grained_thread_pool.h"
#include "../../thread_pool/Task.h"

#include <iostream>
#include <thread>
#include <chrono>

using namespace stepwise;

class test_shared_result : public ::testing::Test {
  public:
    void SetUp() { pool = std::make_unique<fine_grained_thread_pool>(1); }

    task_ptr<int> block{Task<int>::create([]() { return 1; })};

    std::shared_ptr<fine_grained_thread_pool> pool;
};

TEST_F(test_shared_result, reference_count) {
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
        auto res = block->share(pool, task, cond, std::move(notice));
        auto res1 = block->share(pool, task, cond, notice);
        auto res2 = block->share(pool, task, cond, notice);
        auto res3 = block->share(pool, task, cond, notice);
        auto res4 = block->share(pool, task, cond, notice);
        auto res5 = res4;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(log.str() == ans);
}

TEST_F(test_shared_result, vector) {
    constexpr int TASKS_NUMBER = 1000;

    std::vector<stepwise::Task<int>::Result> results;

    std::string ans = "stop";

    std::stringstream msg;
    auto task = [&, step = 0]() mutable -> std::optional<int> {
        msg << "step = " << step << " ";
        ++step;
        return {};
    };

    auto cond = [=]() -> bool { return false; };

    std::stringstream log;
    auto notice = [=, &log]() mutable -> void { log << "stop"; };

    {
        auto res = block->share(pool, task, cond, notice);
        for (int i = 0; i < TASKS_NUMBER; ++i) {
            results.push_back(res);
        }
    }

    results.clear();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(log.str() == ans);
}
