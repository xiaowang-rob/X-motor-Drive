cmake_minimum_required(VERSION 3.22)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(xmath C)
    enable_testing()
    set(XMATH_BACKEND_DEFAULT LIBM)
    set(XMATH_TEST_DEFAULT ON)
else()
    if(NOT DEFINED XMATH_BACKEND_DEFAULT)
        set(XMATH_BACKEND_DEFAULT DSP)
    endif()
    set(XMATH_TEST_DEFAULT OFF)
endif()
set(XMATH_BACKEND "${XMATH_BACKEND_DEFAULT}" CACHE STRING "xmath backend: DSP | LIBM | CORDIC")
set_property(CACHE XMATH_BACKEND PROPERTY STRINGS DSP LIBM CORDIC)
if(NOT XMATH_BACKEND MATCHES "^(DSP|LIBM|CORDIC)$")
    message(FATAL_ERROR "Unsupported xmath backend: ${XMATH_BACKEND}")
endif()

if(NOT XMATH_BACKEND STREQUAL "LIBM")
    # DSP 库与板卡 Cortex-M4 hard-float ABI 一致。
    foreach(required IN ITEMS "${ARM_MATH_ROOT}/Inc/arm_math.h"
                              "${ARM_MATH_ROOT}/Lib/libarm_cortexM4lf_math.a"
                              "${CMSIS_INCLUDE_DIR}/core_cm4.h")
        if(NOT EXISTS "${required}")
            message(FATAL_ERROR "Missing CMSIS-DSP dependency: ${required}")
        endif()
    endforeach()
    add_library(armmath STATIC IMPORTED GLOBAL)
    set_target_properties(armmath PROPERTIES
        IMPORTED_LOCATION "${ARM_MATH_ROOT}/Lib/libarm_cortexM4lf_math.a"
        INTERFACE_INCLUDE_DIRECTORIES "${ARM_MATH_ROOT}/Inc;${CMSIS_INCLUDE_DIR}"
        INTERFACE_COMPILE_DEFINITIONS ARM_MATH_CM4
    )
endif()
# 纯 C 校验工具：无 CMSIS-DSP/HAL 依赖，APP 与 BL 共用。
add_library(xcrc STATIC crc.c)
target_include_directories(xcrc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(xmath STATIC xmath.c filter.c)
target_include_directories(xmath PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(xmath PUBLIC "MATH_LIB_${XMATH_BACKEND}=1")
target_link_libraries(xmath PUBLIC m)
if(NOT XMATH_BACKEND STREQUAL "LIBM")
    target_link_libraries(xmath PUBLIC armmath)
endif()
set_target_properties(xmath PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

option(XMATH_BUILD_TESTS "Build xmath numerical regression tests" ${XMATH_TEST_DEFAULT})
if(XMATH_BUILD_TESTS)
    enable_testing()
    add_executable(xmath_tests tests/test_xmath.c)
    target_link_libraries(xmath_tests PRIVATE xmath)
    target_compile_options(xmath_tests PRIVATE -Wall -Wextra -Werror -Wpedantic -UNDEBUG)
    add_test(NAME xmath_regression COMMAND xmath_tests)
endif()
