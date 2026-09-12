# ============================================================
# XDr-P / xdr_p_app 板级 CMakeLists.txt
#
# 由顶层 firmware/CMakeLists.txt 以 add_subdirectory(board/xdr_p_app) 引入。
#
# 提供目标：
#   stm32_hal    厂商 HAL 库（含 CMSIS 头路径、USE_HAL_DRIVER/STM32F405xx 定义）
#   stm32_core   CubeMX 外设初始化（Core/Src）
#   stm32_usb    USB Device 中间件（含 usbd_cdc_if 等应用层）
#   board_dsp    CMSIS-DSP 头路径与预编译库
#   board_hw     板级资源（板根 *.c + 板级 USB 描述符）
#   board_drv    本板芯片驱动（drv/）
#
# 另向父作用域暴露 BOARD_STARTUP（启动文件，交给可执行目标而非静态库）。
# ============================================================

# ── HAL 库 ──
file(GLOB_RECURSE HAL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/*.c
)
add_library(stm32_hal STATIC ${HAL_SOURCES})
target_include_directories(stm32_hal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
)
target_compile_definitions(stm32_hal PUBLIC
    USE_HAL_DRIVER
    STM32F405xx
    $<$<CONFIG:Debug>:DEBUG>
)

# ── CubeMX 外设初始化 ──
file(GLOB CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/*.c
)
add_library(stm32_core STATIC ${CORE_SOURCES})
target_include_directories(stm32_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_DEVICE/App      # main.c 引 usb_device.h
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_DEVICE/Target
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc
)
target_link_libraries(stm32_core PUBLIC stm32_hal)

# ── USB Device 库 ──
file(GLOB USB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_DEVICE/App/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_DEVICE/Target/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Core/Src/*.c
)
# usbd_desc.c 由板级提供（usb/usbd_desc.c，含 VID/PID），排除 CubeMX 默认的
list(FILTER USB_SOURCES EXCLUDE REGEX "usbd_desc\\.c$")
add_library(stm32_usb STATIC ${USB_SOURCES})
target_include_directories(stm32_usb PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_DEVICE/App
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_DEVICE/Target
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Core/Inc
)
target_link_libraries(stm32_usb PUBLIC stm32_hal)

# ── CMSIS-DSP（头路径 + 预编译库） ──
add_library(board_dsp INTERFACE)
target_include_directories(board_dsp INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/ARM/DSP/Inc
)
target_link_libraries(board_dsp INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/ARM/DSP/Lib/libarm_cortexM4lf_math.a
)

# ── 板级资源：板根 *.c（如 platform.c）+ 板级 USB 描述符 ──
file(GLOB BOARD_ROOT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/usb/*.c
)
add_library(board_hw STATIC ${BOARD_ROOT_SOURCES})
target_include_directories(board_hw PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}          # 板级头（platform.h / board.h …）
    ${CMAKE_CURRENT_SOURCE_DIR}/usb      # USB 描述符
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
)
target_link_libraries(board_hw PUBLIC stm32_hal stm32_core stm32_usb)

# 板级启动文件：交给顶层可执行目标（放进静态库会因归档未被引用而丢失向量表）
set(BOARD_STARTUP ${CMAKE_CURRENT_SOURCE_DIR}/startup_stm32f405xx.s PARENT_SCOPE)

# ── 本板芯片驱动层（drv/） ──
file(GLOB DRV_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/drv/*.c
)
add_library(board_drv STATIC ${DRV_SOURCES})
target_include_directories(board_drv PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/drv
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${APP_INCLUDE_DIRS}                  # 驱动 include app/abs 的语义化契约头
)
target_link_libraries(board_drv PUBLIC stm32_hal stm32_core stm32_usb board_hw board_dsp)
