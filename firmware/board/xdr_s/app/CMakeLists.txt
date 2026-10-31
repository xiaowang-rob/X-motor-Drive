cmake_minimum_required(VERSION 3.22)

# 保留板级独立配置入口，统一转入 firmware_new 的模块组装。
if(NOT DEFINED FW_ROOT)
    set(BOARD xdr_s CACHE STRING "Board")
    set(FW_TYPE app CACHE STRING "Firmware type")
    if(NOT CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/gcc-arm-none-eabi.cmake" CACHE FILEPATH "ARM toolchain")
    endif()
    project(XDr-S C ASM)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../.." "${CMAKE_BINARY_DIR}/firmware")
    return()
endif()

set(FW_TARGET XDr-S)
set(FW_TARGET "${FW_TARGET}" PARENT_SCOPE)
add_executable(${FW_TARGET})

# 迁移期的跨层头路径只承载编译配置，不引入任何实现对象。
add_library(board_headers INTERFACE)
target_include_directories(board_headers INTERFACE
    "${FW_ROOT}/services"
    "${FW_ROOT}/services/abs"
    "${FW_ROOT}/services/utils"
    "${FW_ROOT}/xfoc"
    "${FW_ROOT}/xfoc/abs"
    "${FW_ROOT}/xfoc/ctl"
    "${FW_ROOT}/xfoc/utils"
    "${FW_ROOT}/app"
    "${CMAKE_CURRENT_SOURCE_DIR}/bsp"
    "${CMAKE_CURRENT_SOURCE_DIR}/bsp/sample"
    "${CMAKE_CURRENT_SOURCE_DIR}/bsp/bus"
    "${CMAKE_CURRENT_SOURCE_DIR}/bsp/enc"
    "${CMAKE_CURRENT_SOURCE_DIR}/bsp/flash"
    "${CMAKE_CURRENT_SOURCE_DIR}/bsp/led"
    "${CMAKE_CURRENT_SOURCE_DIR}/bsp/pwm"
    "${CMAKE_CURRENT_SOURCE_DIR}/bsp/uart"
    "${CMAKE_CURRENT_SOURCE_DIR}/bsp/usb"
    "${CMAKE_CURRENT_SOURCE_DIR}/bsp/usb/app"
    "${CMAKE_CURRENT_SOURCE_DIR}/bsp/usb/target"
)
add_subdirectory(cmake/stm32cubemx)
target_link_libraries(board_headers INTERFACE stm32cubemx protocol xmath)

# 仅收集板级驱动；USB core 由 CubeMX 中间件目标单独编译。
file(GLOB_RECURSE BSP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/bsp/*.c")
add_library(bsp OBJECT ${BSP_SOURCES})
target_link_libraries(bsp PUBLIC board_headers)
target_link_libraries(${FW_TARGET} PRIVATE board_headers)
target_link_options(${FW_TARGET} PRIVATE
    "-T${CMAKE_CURRENT_SOURCE_DIR}/STM32G474xx_FLASH.ld"
    "-Wl,-Map=${FW_TARGET}.map"
)
set_target_properties(${FW_TARGET} PROPERTIES ADDITIONAL_CLEAN_FILES "${FW_TARGET}.map")
