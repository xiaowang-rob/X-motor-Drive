# 应用层：app_main_init / app_main_start 由板级 main 调用，不在这里定义 main。
set(MOTOR_DRIVE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

add_library(app OBJECT
        app_main.c
        app_cmd.c
)

target_include_directories(app PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

# 其余头文件由依赖目标传递：protocol 提供协议表，motor_drive 提供算法头，
# motor_services 提供 services/abs/utils，bsp 提供板级驱动头。
target_link_libraries(app PUBLIC protocol motor_drive motor_services bsp)
