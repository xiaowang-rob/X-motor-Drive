# ============================================================
# XDr-S / xdr_s 板级 CMakeLists.txt
#
# 由顶层 firmware/CMakeLists.txt 以 add_subdirectory(board/<BOARD>) 引入
# （BOARD=xdr_s/app）。
#
# 提供目标：
#   stm32_hal    厂商 HAL 库（含 CMSIS 头路径、USE_HAL_DRIVER/STM32G474xx 定义）
#   stm32_core   CubeMX 外设初始化（Core/Src）
#   stm32_usb    USB Device 中间件（含 usbd_cdc_if 等应用层）
#   board_dsp    CMSIS-DSP 头路径与预编译库（本板暂未引入，占位 INTERFACE）
#   board_hw     板级资源（本板暂无板根 *.c / 独立 usb 描述符，占位 INTERFACE）
#   board_drv    本板芯片驱动（drv/）+ 板级支撑层（bsp/）
#
# 另向父作用域暴露 BOARD_STARTUP（启动文件，交给可执行目标而非静态库）。
# ============================================================

# ── HAL 库 ──
file(GLOB_RECURSE HAL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32G4xx_HAL_Driver/Src/*.c
)
add_library(stm32_hal STATIC ${HAL_SOURCES})
target_include_directories(stm32_hal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32G4xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32G4xx_HAL_Driver/Inc/Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32G4xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
)
target_compile_definitions(stm32_hal PUBLIC
    USE_HAL_DRIVER
    STM32G474xx
    $<$<CONFIG:Debug>:DEBUG>
)

# ── CubeMX 外设初始化 ──
file(GLOB CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/*.c
)
add_library(stm32_core STATIC ${CORE_SOURCES})
target_include_directories(stm32_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_Device/App      # main.c 引 usb_device.h
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_Device/Target
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc
)
target_link_libraries(stm32_core PUBLIC stm32_hal)

# ── USB Device 库 ──
# 注：xdr_p 有板级 usb/usbd_desc.c（含 VID/PID），本板暂无，故保留 CubeMX 的
#     USB_Device/App/usbd_desc.c 参与编译。
file(GLOB USB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_Device/App/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_Device/Target/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Core/Src/*.c
)
add_library(stm32_usb STATIC ${USB_SOURCES})
target_include_directories(stm32_usb PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_Device/App
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_Device/Target
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Core/Inc
)
target_link_libraries(stm32_usb PUBLIC stm32_hal)

# ── CMSIS-DSP（占位） ──
# 本板 Middlewares/ST 下暂无 ARM/DSP 预编译库；置为 INTERFACE 以保证顶层
# 目标名存在（app_ctl 若用到 arm_math 需另行引入 DSP 库并改为带 include 的实现）。
add_library(board_dsp INTERFACE)

# ── 板级资源（占位） ──
# 本板暂无板根 *.c 与独立 USB 描述符；后续新增板级源文件时改回 STATIC：
#   file(GLOB BOARD_ROOT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.c)
#   add_library(board_hw STATIC ${BOARD_ROOT_SOURCES})
add_library(board_hw INTERFACE)
target_include_directories(board_hw INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}          # 板级头
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
)
target_link_libraries(board_hw INTERFACE stm32_hal stm32_core stm32_usb)

# 板级启动文件：交给顶层可执行目标（放进静态库会因归档未被引用而丢失向量表）
set(BOARD_STARTUP ${CMAKE_CURRENT_SOURCE_DIR}/startup_stm32g474xx.s PARENT_SCOPE)

# ── 本板芯片驱动层（drv/）+ 板级支撑层（bsp/） ──
# bsp：时间/临界区/集中中断分发等"驱动之下的地基"，唯一接触 HAL 回调符号。
# drv：芯片驱动，只用 bsp 与 HAL 外设类型，不定义 HAL 回调。
file(GLOB DRV_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/drv/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/*.c
)
add_library(board_drv STATIC ${DRV_SOURCES})
target_include_directories(board_drv PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/drv
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${APP_INCLUDE_DIRS}                  # 驱动 include app/abs 的语义化契约头
)
target_link_libraries(board_drv PUBLIC stm32_hal stm32_core stm32_usb board_hw board_dsp)
