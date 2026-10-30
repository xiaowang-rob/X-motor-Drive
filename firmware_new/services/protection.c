
#include "protection.h"

#include "bsp_cfg.h"
#include "bsp_math.h"

#include "parameters.h"

tProtectionManager pro_manager;
// 容忍度检测
static bool _tolerance_check(float value, float max_value, float min_value) {
    if (value > max_value || value < min_value)
        return true;
    return false;
}
// 保护程序 初始化
void pro_manager_init(tParameter *param) {

    pro_manager.fault_flag = false;
    pro_manager.warning_flag = false;
    float tolerance = param->pro_tolerance;
    tolerance = CLAMP(tolerance, 0.01, 1.0);
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
    pro_manager.fault_flag = false;
    pro_manager.warning_flag = false;
}

// 错误处理
void pm_handle_fault(eFault fault) {
    pro_manager.fault_flag = true;
    core_enter_fault(fault);
}

void pm_handle_warning(eWarning warning) {
    pro_manager.warning_flag = true;
    core_enter_warning(warning);
}

// 保护服务主循环
void pro_manager_main_loop(tCore *core) {

    if (pro_manager.fault_flag)
        return;

    // --- 错误监测

    // 过压
    if (core->fb.udc > pro_manager.max_voltage) {
        pm_handle_fault(FAULT_OVERVOLTAGE);
    }

    // 4.CAN通讯异常
    if (!core->com_running) {
        pm_handle_warning(WARNING_BUSCOM_RUN);
    }

    //  4编码器状态检测
    if (!core->obs_running) {
        pm_handle_warning(WARNING_OBS_RUN);
    }
    // 3.电流过大
    if (FABSF(core->fb.is[0]) > MAX_CURRENT ||
        FABSF(core->fb.is[1]) > MAX_CURRENT ||
        FABSF(core->fb.is[2]) > MAX_CURRENT ||
        _tolerance_check(
            core->fb.iq, pro_manager.max_current, -pro_manager.max_current)) {
        pm_handle_fault(FAULT_OVERCURRENT);
    }

    // 警告：
    // 1温度过高
    if (core->fb.temp > pro_manager.max_temp) {
        pm_handle_warning(WARNING_OVERTEMP);
    }

    // 使能之后保护
    if (core->enable) {
        // 2.电压异常
        if (_tolerance_check(core->fb.udc,
                             pro_manager.max_voltage,
                             pro_manager.min_voltage)) {
            if (core->fb.udc > pro_manager.max_voltage) {
                pm_handle_fault(FAULT_OVERVOLTAGE);
            }
            if (core->fb.udc < pro_manager.min_voltage) {
                pm_handle_fault(FAULT_UNDERVOLTAGE);
            }
        }

        // 2 速度检测
        if (_tolerance_check(
                core->fb.vel, pro_manager.max_vel, -pro_manager.max_vel)) {
            pm_handle_warning(WARNING_OVERSPEED);
        }
    }

    //   B错误处理--日志模块还得优化
    if (pro_manager.fault_flag || pro_manager.warning_flag) {
        // log_data_save(&pro_manager);
        //  TODO: 添加错误处理
        //   log_data_write();
        //  TODO: 添加警告处理
        //   log_data_write();
    }
}
