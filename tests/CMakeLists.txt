cmake_minimum_required(VERSION 3.29)    

set(CMAKE_SYSTEM_NAME Generic)

project(test CXX)

set(OS "linux")

if(OS MATCHES "arm")
    set(CMAKE_CXX_COMPILER C:/wtchn/gcc-arm-10.2-2020.11-mingw-w64-i686-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-g++.exe)

    include_directories(c:/wtchn/gcc-arm-10.2-2020.11-mingw-w64-i686-arm-none-linux-gnueabihf/bin/../lib/gcc/arm-none-linux-gnueabihf/10.2.1/include)
    include_directories(c:/wtchn/gcc-arm-10.2-2020.11-mingw-w64-i686-arm-none-linux-gnueabihf/bin/../lib/gcc/arm-none-linux-gnueabihf/10.2.1/include-fixed)
    include_directories(c:/wtchn/gcc-arm-10.2-2020.11-mingw-w64-i686-arm-none-linux-gnueabihf/bin/../lib/gcc/arm-none-linux-gnueabihf/10.2.1/../../../../arm-none-linux-gnueabihf/include)
    include_directories(c:/wtchn/gcc-arm-10.2-2020.11-mingw-w64-i686-arm-none-linux-gnueabihf/bin/../arm-none-linux-gnueabihf/libc/usr/include)
elseif(OS MATCHES "linux")
    set(CMAKE_CXX_COMPILER /usr/bin/g++)
endif()

add_executable(test_main test_main.cpp)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(../../googletest/googletest/include)
include_directories(../../googletest/googlemock/include)

include_directories(../ThreadPool)

if(OS MATCHES "arm")
    target_link_libraries(test_main ${CMAKE_CURRENT_SOURCE_DIR}/../../googletest/build/lib/libgtest.a)
    target_link_libraries(test_main ${CMAKE_CURRENT_SOURCE_DIR}/../../googletest/build/lib/libgmock.a)
    target_link_libraries(test_main ${CMAKE_CURRENT_SOURCE_DIR}/../../googletest/build/lib/libgmock_main.a)
    target_link_libraries(test_main ${CMAKE_CURRENT_SOURCE_DIR}/../../googletest/build/lib/libgtest_main.a)
elseif(OS MATCHES "linux")
    target_link_libraries(test_main /home/kali/googletest/lib/libgtest.a)
    target_link_libraries(test_main /home/kali/googletest/lib/libgmock.a)
    target_link_libraries(test_main /home/kali/googletest/lib/libgmock_main.a)
    target_link_libraries(test_main /home/kali/googletest/lib/libgtest_main.a)
endif()