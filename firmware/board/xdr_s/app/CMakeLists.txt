cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME XDr-S)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# ── xfoc 算法库（仓库根 firmware/xfoc）──
# 各板独立编译，xfoc 只编一次、链进本板的 XDr-S。
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../xfoc ${CMAKE_BINARY_DIR}/xfoc)

# xfoc 的平台头会间接引用 main.h 和 HAL；通过 CubeMX 接口目标获取
# Core/Inc、HAL/CMSIS 路径及芯片宏。该目标仅携带编译配置，不引入板级实现。
target_link_libraries(xfoc PRIVATE stm32cubemx)

# 板级平台契约头与 CMSIS-DSP 路径只在 xfoc 内部可见。
target_include_directories(xfoc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/pwm
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/ARM/DSP/Inc
    # ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Class/CustomHID/Inc
    # ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32G4xx/Include
)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
# 板级 bsp 驱动：各子目录扁平 include（"bsp_adc.h" 等），
# 源文件与头文件都在 bsp/ 下，按目录收进来即可。
file(GLOB BSP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/sample/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/bus/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/enc/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/enc/abi/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/enc/hall/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/enc/sincos/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/enc/spi/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/flash/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/led/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/pwm/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/uart/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/usb/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/usb/app/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/usb/Core/Src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/usb/target/*.c
)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${BSP_SOURCES})

# Add include paths
# bsp 各层使用扁平 include（"bsp_adc.h" / "bsp_bus.h" 等），
# 因此根目录和各子目录都进搜索路径；Core/Inc 由 stm32cubemx
# 这个 INTERFACE target 的 PUBLIC include 自动带进来。
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/sample
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/bus
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/enc
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/flash
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/led
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/pwm
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/uart
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/usb
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/usb/app
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/usb/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp/usb/target
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
# target_link_libraries(${CMAKE_PROJECT_NAME}
#     stm32cubemx
#     xfoc

#     # Add user defined libraries
# )
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    xfoc
    :libarm_cortexM4lf_math.a
    ${TOOLCHAIN_LINK_LIBRARIES}
)
