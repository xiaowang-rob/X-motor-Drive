
#include "protection.h"
#include "bsp_cfg.h"
#include "xmath.h"

tProtectionManager pro_manager;
// 容忍度检测
static bool _tolerance_check(float value, float max_value, float min_value) {
    if (value > max_value || value < min_value)
        return true;
    return false;
}
// 保护程序 初始化
void pro_manager_init(tParameter *param) {

    float tolerance = param->pro_tolerance;
    tolerance = XCLAMP(tolerance, 0.01, 1.0);
    tolerance += 1;
    pro_manager.max_current = param->motor_rated_current;
    pro_manager.max_vel = param->velocity_limit * tolerance;
    pro_manager.min_voltage =
        param->undervoltage_threshold * tolerance < MIN_VOLTAGE
            ? param->undervoltage_threshold * tolerance
            : MIN_VOLTAGE;
    pro_manager.max_voltage =
        param->overvoltage_threshold * tolerance > MAX_VOLTAGE
            ? param->overvoltage_threshold * tolerance
            : MAX_VOLTAGE;
    pro_manager.max_temp =
        param->overtemp_threshold * tolerance > MAX_TEMPERATURE
            ? param->overtemp_threshold * tolerance
            : MAX_TEMPERATURE;
    pro_manager.tolerance_time_ms = param->pro_tolerance_time_ms;
}

// 保护程序复位
void pro_manager_reset(void) {

    pro_manager.fault = FAULT_NONE;
    motor_reset();
}

// 错误处理
void pm_handle_fault(eFault fault) {

    if (pro_manager.fault != FAULT_NONE)
        return;
    pro_manager.fault = fault;
    motor_handle_fault();
    //   B错误处理--日志模块还得优化

    // log_data_save(&pro_manager);
    //  TODO: 添加错误处理
    //   log_data_write();
    //  TODO: 添加警告处理
    //   log_data_write();
}

// 保护服务主循环
void pm_loop_task(tMotorCtrl *motor) {

    if (pro_manager.fault != FAULT_NONE)
        return;

    // --- 错误监测

    // 过压
    if (motor->fb.udc > pro_manager.max_voltage) {
        pm_handle_fault(FAULT_OVERVOLTAGE);
    }

    //  4编码器状态检测
    // TODO: 后续把obs 划分功能改到服务层
    // 3.电流过大
    if (XFABSF(motor->fb.is[0]) > MAX_CURRENT ||
        XFABSF(motor->fb.is[1]) > MAX_CURRENT ||
        XFABSF(motor->fb.is[2]) > MAX_CURRENT ||
        _tolerance_check(
            motor->fb.iq, pro_manager.max_current, -pro_manager.max_current)) {
        pm_handle_fault(FAULT_OVERCURRENT);
    }

    // 警告：
    // 1温度过高
    if (motor->fb.temp > pro_manager.max_temp) {
        pm_handle_fault(FAULT_OVERTEMP);
    }

    // 使能之后保护
    if (motor->cfg.cur_enable) {
        // 2.电压异常
        if (_tolerance_check(motor->fb.udc,
                             pro_manager.max_voltage,
                             pro_manager.min_voltage)) {
            if (motor->fb.udc > pro_manager.max_voltage) {
                pm_handle_fault(FAULT_OVERVOLTAGE);
            }
            if (motor->fb.udc < pro_manager.min_voltage) {
                pm_handle_fault(FAULT_UNDERVOLTAGE);
            }
        }

        // 2 速度检测
        if (_tolerance_check(
                motor->fb.vel, pro_manager.max_vel, -pro_manager.max_vel)) {
            pm_handle_fault(FAULT_OVERSPEED);
        }
    }
}

// TODO:添加日志功能
