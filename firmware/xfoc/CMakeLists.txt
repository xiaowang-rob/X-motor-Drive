# ============================================================
# xfoc — 电机控制算法库（与具体板卡无关）
#
# 由各板级工程（board/<board>/<type>/CMakeLists.txt）通过
# add_subdirectory 引入，编译成静态库后链进该板的可执行目标。
#
# 本文件只声明 xfoc 自己的源文件与公开头路径；板级平台契约头
# （bsp_cfg.h / bsp_math.h / bsp_time.h / bsp_irq.h）和 CMSIS-DSP
# 头由板级在引入后以 PRIVATE 方式注入，避免 board 与 xfoc 形成
# target 级循环依赖。
# ============================================================

file(GLOB XFOC_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/abs/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ctl/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/srv/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/*.c
)

add_library(xfoc STATIC ${XFOC_SOURCES})

target_include_directories(xfoc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/abs
    ${CMAKE_CURRENT_SOURCE_DIR}/ctl
    ${CMAKE_CURRENT_SOURCE_DIR}/srv
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../protocol
)

set_target_properties(xfoc PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)
