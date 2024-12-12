#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../../thread_pool/fine_grained_thread_pool.h"

#include <chrono>

class test_fine_grained_thread_pool : public ::testing::Test {
  public:
    void SetUp() { pool = std::make_unique<fine_grained_thread_pool>(1); }

    std::unique_ptr<fine_grained_thread_pool> pool;
};

TEST_F(test_fine_grained_thread_pool, two_tasks) {
    std::string ans = {"func 1 new max_prime = 3\nfunc 1 new max_prime = 5\nfunc 1 new max_prime = 7\nfunc 2 new "
                       "max_prime = 3\nfunc 2 new max_prime = 5\nfunc 2 new max_prime = 7\n"};
    std::stringstream log;

    auto func1 = [&log]() mutable -> bool {
        int max_prime = 2;
        for (int i = 3; i < 10; ++i) {
            bool flag = true;
            for (int j = 2; j <= sqrt(i + 0.0) + 1; ++j) {
                if (i % j == 0) {
                    flag = false;
                    break;
                }
            }

            if (flag) {
                if (i > max_prime) {
                    max_prime = i;
                }
                log << "func 1 new max_prime = " + std::to_string(max_prime) << std::endl;
            }
        }
        return true;
    };

    auto func2 = [&log]() mutable -> bool {
        int max_prime = 2;
        for (int i = 3; i < 10; ++i) {
            bool flag = true;
            for (int j = 2; j <= sqrt(i + 0.0) + 1; ++j) {
                if (i % j == 0) {
                    flag = false;
                    break;
                }
            }

            if (flag) {
                if (i > max_prime) {
                    max_prime = i;
                }
                log << "func 2 new max_prime = " + std::to_string(max_prime) << std::endl;
            }
        }
        return true;
    };

    auto f1 = pool->submit(func1);
    auto f2 = pool->submit(func2);
    f1.wait();
    f2.wait();

    ASSERT_TRUE(log.str() == ans);
}

TEST_F(test_fine_grained_thread_pool, two_stepwise_tasks) {
    std::string ans = "func 1 step 0\nfunc 2 step 0\nfunc 1 step 1\nfunc 2 step 1\nfunc 1 step 2\nfunc 2 step 2\nfunc "
                      "1 step 3\nfunc 2 step 3\nfunc 1 step 4\nfunc 2 step 4\n";
    std::stringstream log;

    auto func1 = [&log, i = 0]() mutable -> std::optional<bool> {
        for (; i < 5;) {
            log << "func 1 step " << std::to_string(i) << std::endl;
            ++i;
            std::this_thread::yield();
            return {};
        }
        return {true};
    };

    auto func2 = [&log, j = 0]() mutable -> std::optional<bool> {
        for (; j < 5;) {
            log << "func 2 step " << std::to_string(j) << std::endl;
            ++j;
            std::this_thread::yield();
            return {};
        }
        return {true};
    };

    auto f1 = pool->submit(func1);
    auto f2 = pool->submit(func2);
    f1.wait();
    f2.wait();

    ASSERT_TRUE(log.str() == ans);
}

TEST_F(test_fine_grained_thread_pool, kill_stepwise_tasks) {
    std::stringstream log;
    std::atomic_bool flag = false;

    auto func = [&log, i = 0]() mutable -> std::optional<bool> {
        while (true) {
            log << "func step " << std::to_string(i) << std::endl;
            ++i;
            return {};
        }
        return {true};
    };

    auto cond = [&flag]() -> bool { return flag; };

    auto f = pool->submit(func, cond);
    std::this_thread::yield();
    flag = true;

    ASSERT_THROW(f.get(), stepwise::bad_value);
}
