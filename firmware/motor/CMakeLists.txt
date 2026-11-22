# 可单独构建算法/主机测试，不依赖 BSP、services 或 CMSIS。
cmake_minimum_required(VERSION 3.22)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(motor_control C)
    enable_testing()
    option(MC_BUILD_TESTS "Build motor host regression tests" ON)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../protocol" "${CMAKE_CURRENT_BINARY_DIR}/protocol")
    set(XMATH_BACKEND_DEFAULT LIBM)
    set(XMATH_BUILD_TESTS ${MC_BUILD_TESTS} CACHE BOOL "Build xmath numerical regression tests")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../math" "${CMAKE_CURRENT_BINARY_DIR}/xmath")
    set(MC_TEST_DEFAULT ON)
else()
    set(MC_TEST_DEFAULT OFF)
endif()
option(MC_BUILD_TESTS "Build motor host regression tests" ${MC_TEST_DEFAULT})

add_library(motor STATIC
    mc_motor.c
    core/mc_config.c
    command/mc_command.c
    common/trajectory.c
    common/pi.c
    common/pid.c
    common/mit.c
    control/mc_controller.c
    control/mc_foc.c
    control/mc_sensor.c
)
target_include_directories(motor PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(motor PUBLIC protocol xmath)
set_target_properties(motor PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

if(MC_BUILD_TESTS)
    enable_testing()
    add_executable(motor_tests tests/test_motor.c)
    target_link_libraries(motor_tests PRIVATE motor)
    target_compile_options(motor PRIVATE -Wall -Wextra -Werror -Wpedantic)
    target_compile_options(motor_tests PRIVATE -Wall -Wextra -Werror -Wpedantic -UNDEBUG)
    add_test(NAME motor_regression COMMAND motor_tests)
endif()
