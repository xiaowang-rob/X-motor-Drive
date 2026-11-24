add_subdirectory(protocol)

# ---------------------------------------------------------------------------
# 抽象接口层：abs/ 里的设备抽象头 + utils/ 里的通用工具头。
# BSP 与业务层都只依赖这里的声明，不直接依赖对方实现。
# ---------------------------------------------------------------------------
add_library(motor_interfaces INTERFACE)
target_include_directories(motor_interfaces INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/abs"
        "${CMAKE_CURRENT_SOURCE_DIR}/utils"
)
target_link_libraries(motor_interfaces INTERFACE protocol)

# ---------------------------------------------------------------------------
# 电机算法层。observer/reference/ 是重构前的参考实现，按该目录 README 的
# 约定不进入构建。
# ---------------------------------------------------------------------------
add_library(motor_drive STATIC
        motor/mc_motor.c
        motor/command/mc_command.c
        motor/common/mit.c
        motor/common/pid.c
        motor/common/pll.c
        motor/common/trajectory.c
        motor/control/mc_controller.c
        motor/control/mc_motion.c
        motor/control/mc_pwm.c
        motor/core/mc_config.c
        motor/sensor/mc_sensor.c
        utils/crc.c
        utils/filter.c
)

target_include_directories(motor_drive PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/motor"
        "${CMAKE_CURRENT_SOURCE_DIR}/utils"
)

target_link_libraries(motor_drive PUBLIC bsp protocol)
target_compile_definitions(motor_drive PRIVATE $<$<NOT:$<CONFIG:Debug>>:NDEBUG>)

# ---------------------------------------------------------------------------
# 服务层：通用工具、设备抽象实现、业务服务。
# ---------------------------------------------------------------------------
add_library(motor_services STATIC
        utils/memory_pool.c
        utils/queue.c
        utils/tune_alg.c
        abs/bus_com.c
        abs/enc.c
        abs/led.c
        abs/pwm.c
        abs/sample.c
        abs/storage.c
        abs/uart_com.c
        abs/usb_com.c
        services/comm.c
        services/enc_manager.c
        services/gate_drv.c
        services/obsdata.c
        services/parameters.c
        services/protection.c
        services/tune.c
)

target_include_directories(motor_services PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/services"
)

target_link_libraries(motor_services PUBLIC motor_drive motor_interfaces)

# ---------------------------------------------------------------------------
# 应用层：组装上面各层，提供 app_main_* 入口（不定义 main）。
# ---------------------------------------------------------------------------
add_subdirectory(app)
