#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../../thread_pool/fine_grained_thread_pool.h"
#include "../../thread_pool/Task.h"
#include "../../connection/QueueConnection.h"

#include <iostream>
#include <thread>
#include <chrono>

using namespace stepwise;

class test_queue_connection : public ::testing::Test {
  public:
    void SetUp() { pool = std::make_unique<fine_grained_thread_pool>(1); }

    std::shared_ptr<fine_grained_thread_pool> pool;
};

TEST_F(test_queue_connection, data_transfer) {
    std::shared_ptr<QueueConnectionSender<std::string>> sender{new QueueConnectionSender<std::string>(5)};
    std::shared_ptr<IConnectionReceiver<std::string>> receiver = sender->getReceiver();

    auto writer = [=, step = 0]() mutable -> std::optional<int> {
        std::cout << "writer works" << std::endl;
        if (step == 0) {
            sender->send("Hello, ");
            ++step;
            return {};
        }

        if (step == 1) {
            sender->send("connection ");
            ++step;
            return {};
        }

        sender->send("receiver. ");
        ++step;
        sender->close();

        return {step};
    };

    auto reader = [=, res = std::string("")]() mutable -> std::optional<std::string> {
        std::cout << "reader works" << std::endl;

        try {
            auto new_data = receiver->receive();

            res += new_data.value_or("");
            return {};
        } catch (std::exception &e) {
            res += std::string(e.what());
            return {res};
        }
    };

    auto readerTask = Task<std::string>::create(reader);
    auto readerResult = readerTask->share(pool);

    auto writerTask = Task<int>::create(writer);
    auto writerResult = writerTask->share(pool);

    ASSERT_TRUE(writerResult.get() == 3);
    auto res = readerResult.get();
    std::cout << res << std::endl;
    ASSERT_TRUE(res == "Hello, connection receiver. the sender is closed, there will be no more data");
}
