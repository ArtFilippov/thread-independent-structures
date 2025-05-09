#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "thread_pool/test_fine_grained_thread_pool.h"
#include "thread_pool/test_shared_result.h"
#include "connection/test_connection.h"
// #include "thread_pool/test_task_manager.h"

int main(int argc, char *argv[]) {
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
