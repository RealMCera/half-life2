cmake_minimum_required(VERSION 3.20)
project(host_prof_test C)
# The in-process sampler (src/platform/host_prof.c) needs only the host's
# thread calls, so the test builds the platform files it uses, on the hosts
# that have a sampler: Windows (and Wine) and macOS.
if(NOT WIN32 AND NOT APPLE)
    message(FATAL_ERROR "host_prof samples on Windows and macOS only")
endif()
set(CMAKE_C_STANDARD 11)
set(P ../../src/platform)
add_executable(host_prof_test test_main.c ${P}/host_prof.c ${P}/recomp_env.c
    ${P}/recomp_exe_dir.c ${P}/recomp_exit_hook.c ${P}/host_time.c)
target_include_directories(host_prof_test PRIVATE ${P} ../../src)
if(WIN32)
    target_link_libraries(host_prof_test PRIVATE winmm)
endif()
enable_testing()
add_test(NAME host_prof_test COMMAND host_prof_test)
