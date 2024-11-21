#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ThreadPool/test_fine_grained_thread_pool.h"

int main(int argc, char *argv[]) {
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
