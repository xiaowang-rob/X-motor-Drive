# ============================================================
# X-motor-Drive 固件顶层 CMakeLists.txt
#
# 固件 = 应用侧（app/ 或 bootload/） + 板级（board/<BOARD>/）
#
# 手动配置（在仓库根目录执行）：
#   cmake -S firmware -B firmware/build \
#         -DBOARD=xdr_p_app -DFW_TYPE=app \
#         -DCMAKE_BUILD_TYPE=Debug \
#         -DCMAKE_TOOLCHAIN_FILE=$PWD/firmware/board/xdr_p_app/cmake/gcc-arm-none-eabi.cmake
#   cmake --build firmware/build -j
#
# 或使用 preset：
#   cmake --preset xdr_p_app-app   （在 firmware/ 下）
#
# 目录：
#   board/<BOARD>/   板级：HAL + CubeMX Core + USB 中间件 + drv 驱动 + startup
#   app/             应用侧：abs 抽象 / app 组装 / ctl 控制 / srv 服务 / utils 工具
#   bootload/        引导固件源码（FW_TYPE=bl）
#   ../protocol/     上下位机通讯协议头
# ============================================================

cmake_minimum_required(VERSION 3.22)

project(XDr C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# ── 目标选择 ──
set(BOARD "xdr_p_app" CACHE STRING "板级目录名，对应 firmware/board/<BOARD>/")
set(FW_TYPE "app" CACHE STRING "固件类型：app（应用固件）| bl（引导固件）")
set_property(CACHE FW_TYPE PROPERTY STRINGS app bl)

set(FW_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(BOARD_DIR ${FW_ROOT}/board/${BOARD})
set(PROTOCOL_DIR ${FW_ROOT}/../protocol)

if(NOT IS_DIRECTORY ${BOARD_DIR})
    message(FATAL_ERROR "找不到板级目录：${BOARD_DIR}（BOARD=${BOARD}）")
endif()

# ── 应用侧头文件搜索路径 ──
# 注：应用侧源码使用扁平 include（如 "device.h" / "math_fast.h" / "encoder_drivers.h"），
#     因此各分层目录与板级 drv 目录都需进入搜索路径。
set(APP_INCLUDE_DIRS
    ${FW_ROOT}/app/abs
    ${FW_ROOT}/app/app
    ${FW_ROOT}/app/ctl
    ${FW_ROOT}/app/srv
    ${FW_ROOT}/app/utils
    ${BOARD_DIR}
    ${BOARD_DIR}/drv
    ${PROTOCOL_DIR}
)

# ── 板级层：stm32_hal / stm32_core / stm32_usb / board_drv / board_hw ──
add_subdirectory(board/${BOARD})

# ── 应用侧分层 ──
# 用 OBJECT 库：对象直接进可执行目标，免静态库归档顺序与分层间循环依赖问题。
function(fw_add_layer name dir)
    if(NOT IS_DIRECTORY ${dir})
        message(FATAL_ERROR "找不到分层目录：${dir}")
    endif()
    file(GLOB LAYER_SOURCES ${dir}/*.c)
    list(LENGTH LAYER_SOURCES _n)
    add_library(${name} OBJECT ${LAYER_SOURCES})
    target_include_directories(${name} PUBLIC ${APP_INCLUDE_DIRS})
    # 只取 include/宏（stm32_hal/board_dsp）；drv 的头已由 APP_INCLUDE_DIRS 覆盖，
    # 其实现符号在最终可执行目标处链接，避免分层编译被板级目标阻塞。
    target_link_libraries(${name} PUBLIC stm32_hal board_dsp)
    message(STATUS "    分层 ${name}: ${_n} 个源文件  (${dir})")
endfunction()

message(STATUS "XDr 固件构建：BOARD=${BOARD}  FW_TYPE=${FW_TYPE}")
message(STATUS "  应用侧分层：")

fw_add_layer(app_abs   ${FW_ROOT}/app/abs)
fw_add_layer(app_utils ${FW_ROOT}/app/utils)
fw_add_layer(app_ctl   ${FW_ROOT}/app/ctl)
fw_add_layer(app_srv   ${FW_ROOT}/app/srv)
fw_add_layer(app_app   ${FW_ROOT}/app/app)

set(APP_LAYERS app_abs app_utils app_ctl app_srv app_app)

# ── 固件可执行目标 ──
# 启动文件来自板级（board_hw 通过 BOARD_STARTUP 变量向本作用域暴露）
if(FW_TYPE STREQUAL "bl")
    file(GLOB BL_SOURCES ${FW_ROOT}/bootload/*.c)
    list(LENGTH BL_SOURCES _n)
    message(STATUS "  引导侧：${_n} 个源文件  (${FW_ROOT}/bootload)")

    add_executable(XDr-BL ${BL_SOURCES} ${BOARD_STARTUP})
    target_include_directories(XDr-BL PUBLIC ${APP_INCLUDE_DIRS} ${FW_ROOT}/bootload)
    target_link_libraries(XDr-BL PUBLIC board_drv board_hw stm32_usb stm32_core stm32_hal board_dsp)
else()
    add_executable(XDr ${BOARD_STARTUP})
    target_link_libraries(XDr PUBLIC
        board_drv board_hw stm32_usb stm32_core stm32_hal board_dsp
    )
    # OBJECT 库：显式并入全部对象，保证 app/ 下每个 .c 都参与链接
    foreach(layer ${APP_LAYERS})
        target_sources(XDr PRIVATE $<TARGET_OBJECTS:${layer}>)
    endforeach()
endif()
