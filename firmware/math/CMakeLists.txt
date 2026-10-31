# 数学库先于算法和应用注册；DSP 库与板卡 Cortex-M4 hard-float ABI 一致。
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
add_library(xmath STATIC xmath.c)
target_include_directories(xmath PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(xmath PUBLIC armmath m)
