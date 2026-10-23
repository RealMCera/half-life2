cmake_minimum_required(VERSION 3.20)
project(kick_wake_test C)
# The walker's event wait (nv2a_kick_wake.c) over plain words and the host
# waker (host_time.c): no runtime, so it builds alone. Its guest and walker
# are pthreads, so a POSIX host runs it.
if(WIN32)
    message(FATAL_ERROR "kick_wake_test drives the protocol with pthreads: POSIX hosts only")
endif()
set(CMAKE_C_STANDARD 11)
add_executable(kick_wake_test test_main.c
    ../../src/kernel/nv2a_kick_wake.c ../../src/platform/host_time.c)
target_include_directories(kick_wake_test PRIVATE ../../src ../../src/kernel ../../src/platform)
find_package(Threads REQUIRED)
target_link_libraries(kick_wake_test PRIVATE Threads::Threads)
enable_testing()
add_test(NAME kick_wake_test COMMAND kick_wake_test)
