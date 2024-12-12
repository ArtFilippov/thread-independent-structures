#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../../thread_pool/fine_grained_thread_pool.h"
#include "../../thread_pool/shared_result.h"

#include <iostream>

typedef stepwise::shared_result_control_block<int> control_int;
typedef std::shared_ptr<control_int> shared_control_int;

class test_shared_result : public ::testing::Test {
  public:
    void SetUp() { pool = std::make_unique<fine_grained_thread_pool>(1); }

    shared_control_int block{std::make_shared<control_int>()};

    std::unique_ptr<fine_grained_thread_pool> pool;
};

TEST_F(test_shared_result, reference_count) {
    std::string ans = "stop";

    std::stringstream msg;
    auto task = [&, step = 0]() mutable -> std::optional<int> {
        msg << "step = " << step << " ";
        ++step;
        return {};
    };

    auto cond = [=]() -> bool { return !block->does_it_expect(); };

    std::stringstream log;
    auto notice = [=, &log]() mutable -> void {
        log << "stop";
        block->notify_about_readiness();
    };

    {
        auto res = block->new_share(pool->submit(task, cond, notice));
        auto res1 = block->share();
        auto res2 = block->share();
        auto res3 = block->share();
        auto res4 = block->share();
        auto res5 = res4;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(log.str() == ans);
}

TEST_F(test_shared_result, vector) {
    constexpr int TASKS_NUMBER = 1000;

    std::vector<stepwise::shared_result<int>> results;

    std::string ans = "stop";

    std::stringstream msg;
    auto task = [&, step = 0]() mutable -> std::optional<int> {
        msg << "step = " << step << " ";
        ++step;
        return {};
    };

    auto cond = [=]() -> bool { return !block->does_it_expect(); };

    std::stringstream log;
    auto notice = [=, &log]() mutable -> void {
        log << "stop";
        block->notify_about_readiness();
    };

    {
        auto res = block->new_share(pool->submit(task, cond, notice));
        for (int i = 0; i < TASKS_NUMBER; ++i) {
            results.push_back(res);
        }
    }

    results.clear();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(log.str() == ans);
}
