#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../../ThreadPool/fine_grained_thread_pool.h"
#include "../../ThreadPool/shared_result.h"

#include <iostream>

class test_shared_result : public ::testing::Test {
  public:
    void SetUp() { pool = std::make_unique<fine_grained_thread_pool>(1); }

    stepwise::shared_result_control_block<int> block{};

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

    auto cond = [&]() -> bool { return !block.does_it_expect(); };

    std::stringstream log;
    auto notice = [&]() mutable -> void {
        log << "stop";
        block.notify_about_readiness();
    };

    {
        auto res = block.new_share(pool->submit(task, cond, notice));
        auto res1 = block.share();
        auto res2 = block.share();
        auto res3 = block.share();
        auto res4 = block.share();
        auto res5 = res4;

        std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << msg.str() << std::endl;
    std::cout << log.str() << std::endl;

    ASSERT_TRUE(log.str() == ans);
}
