cmake_minimum_required(VERSION 3.22)

# 统一入口；也允许从 board/<BOARD>/<FW_TYPE> 单独配置。
set(FW_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(BOARD "xdr_s" CACHE STRING "Board: xdr_s | xdr_p")
set(FW_TYPE "app" CACHE STRING "Firmware type: app | bl")
set_property(CACHE BOARD PROPERTY STRINGS xdr_s xdr_p)
set_property(CACHE FW_TYPE PROPERTY STRINGS app bl)
if(NOT BOARD MATCHES "^(xdr_s|xdr_p)$" OR NOT FW_TYPE MATCHES "^(app|bl)$")
    message(FATAL_ERROR "Unsupported firmware selection: BOARD=${BOARD}, FW_TYPE=${FW_TYPE}")
endif()
set(BOARD_DIR "${FW_ROOT}/board/${BOARD}/${FW_TYPE}")
if(NOT EXISTS "${BOARD_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Board project is not generated: ${BOARD_DIR}; generate it with STM32CubeMX first")
endif()
if(NOT FW_TYPE STREQUAL "app")
    message(FATAL_ERROR "Only the new app build environment is available")
endif()

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${BOARD_DIR}/cmake/gcc-arm-none-eabi.cmake" CACHE FILEPATH "ARM toolchain")
endif()
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type")
endif()
project(XDr C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 基础依赖先注册；xmath 使用所选板卡配套的 CMSIS-DSP。
set(ARM_MATH_ROOT "${BOARD_DIR}/Middlewares/ST/ARM/DSP")
set(CMSIS_INCLUDE_DIR "${BOARD_DIR}/Drivers/CMSIS/Include")
add_subdirectory("${FW_ROOT}/protocol" "${CMAKE_BINARY_DIR}/protocol")
add_subdirectory("${FW_ROOT}/math" "${CMAKE_BINARY_DIR}/xmath")
add_subdirectory("${FW_ROOT}/xfoc" "${CMAKE_BINARY_DIR}/xfoc")

# board 保留 Core/Src/main.c 和 startup，不创建第二个 main。
add_subdirectory("${BOARD_DIR}" "${CMAKE_BINARY_DIR}/board")
add_subdirectory("${FW_ROOT}/services" "${CMAKE_BINARY_DIR}/services")
add_subdirectory("${FW_ROOT}/app" "${CMAKE_BINARY_DIR}/app")

# 源码迁移尚未完成，旧头文件仍跨层引用。只提供编译期头路径，
# 不让 xfoc 链接 BSP/services 的实现；后续在源码迁移时移除这些依赖。
target_link_libraries(xfoc PRIVATE board_headers)

# OBJECT 层显式并入固件，避免 BSP 回调和服务间引用的静态归档顺序问题。
target_link_libraries(${FW_TARGET} PRIVATE app services bsp xfoc xmath)
