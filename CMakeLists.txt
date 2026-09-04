# ============================================================
# X-motor-Drive 顶层 CMakeLists.txt —— v2：drv 直连厂商库
#
# 库目标：
#   usr_abs  业务对象（纯逻辑，可 host 编译）
#   usr_drv  芯片/板驱动（直调厂商库，经 board/<b>/platform.h）
#   usr_app  组装层（dev_board → g_dev）
#   board_hw/stm32_* 板级（platform + HAL/Core/USB）
#
# 配置（toolchain 需绝对路径，且显式给编译器）：
#   cmake -S . -B build -G "Unix Makefiles" \
#     -DCMAKE_TOOLCHAIN_FILE=$PWD/board/xdr_p_o1.2/cmake/gcc-arm-none-eabi.cmake \
#     -DCMAKE_MAKE_PROGRAM=/usr/bin/make \
#     -DCMAKE_C_COMPILER=arm-none-eabi-gcc -DCMAKE_ASM_COMPILER=arm-none-eabi-gcc
#   cmake --build build --target usr_app
# ============================================================

cmake_minimum_required(VERSION 3.22)
project(XDr C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(USR_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(BOARD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/board/xdr_p_o1.2)

# ── usr/abs 业务对象层（纯逻辑；类型含 abs↔drv 契约） ──
file(GLOB ABS_SOURCES usr/abs/*.c)
add_library(usr_abs STATIC ${ABS_SOURCES})
target_include_directories(usr_abs PUBLIC ${USR_ROOT})

# ── 板级层（先于 drv：stm32_hal 的 PUBLIC include/宏 供 drv 使用） ──
add_subdirectory(board/xdr_p_o1.2)

# ── usr/drv 驱动层（v2：直连厂商库；include 根 + 本板目录拿 platform.h） ──
file(GLOB DRV_SOURCES usr/drv/*.c)
add_library(usr_drv STATIC ${DRV_SOURCES})
target_include_directories(usr_drv PUBLIC ${USR_ROOT} ${BOARD_DIR})
target_link_libraries(usr_drv PUBLIC stm32_hal) # HAL 头/宏/类型（含 USE_HAL_DRIVER 等）

# ── usr/app 组装层：装配 g_dev ──
# 注：usr/app 下旧高层文件（app_main/can_port/...）尚未迁移，暂只编 dev_board
set(APP_SOURCES usr/app/dev_board.c)
add_library(usr_app STATIC ${APP_SOURCES})
target_include_directories(usr_app PUBLIC ${USR_ROOT} ${BOARD_DIR})
target_link_libraries(usr_app PUBLIC usr_abs usr_drv board_hw)
