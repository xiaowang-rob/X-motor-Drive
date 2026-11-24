#include "protection.h"

#include "bsp_config.h"
#include "bsp_math.h"

#include "bsp_led.h"
#include "bsp_flash.h"

// 容忍度检测
static bool tolerance_check(float value, float max_value, float min_value) {
    if (value > max_value || value < min_value)
        return true;
    return false;
}

const static eIndicator_state state_table[] = {
    [STATE_IDLE] = INDICATOR_STATE_IDLE,
    [STATE_FAULT] = INDICATOR_STATE_FAULT,
    [STATE_RUNNING] = INDICATOR_STATE_RUNNING0,
};


static inline void status_fb_loop_task(tProtectionManager *pm) {
    led_set_state(&pm->led, state_table[pm->state]);
    led_task(&pm->led);
}

__attribute__((weak)) void PM_error_Callback(tProtectionManager *pm) {
    (void) pm;
}

// 保护程序 初始化
bool protection_init(tProtectionManager *pm) {
    if (!storage_init(&pm->log_storage, Get_LogPartHandle()))
        return false;
    if (!led_init(&pm->led, Get_Gpio_LEDHandle()))
        return false;
}

void protection_config(tProtectionManager *pm, tParameter *param) {
    float tolerance = param->pro_tolerance;
    tolerance = MATH_CLAMP(tolerance, 0.01, 1.0);
    tolerance += 1;
    pm->max_current = param->motor_rated_current * tolerance < MAX_CURRENT
                          ? param->motor_rated_current * tolerance
                          : MAX_CURRENT;
    pm->max_vel = param->velocity_limit * tolerance;
    pm->min_voltage =
            param->undervoltage_threshold * tolerance > MIN_VOLTAGE
                ? param->undervoltage_threshold * tolerance
                : MIN_VOLTAGE;
    pm->max_voltage =
            param->overvoltage_threshold * tolerance < MAX_VOLTAGE
                ? param->overvoltage_threshold * tolerance
                : MAX_VOLTAGE;
    pm->max_temp =
            param->overtemp_threshold * tolerance < MAX_TEMPERATURE
                ? param->overtemp_threshold * tolerance
                : MAX_TEMPERATURE;

    pm->tolerance_time_ms = param->pro_tolerance_time_ms;
}

// 保护程序复位
void protection_reset(tProtectionManager *pm, tMotorCtrl *motor) {
    pm->fault = FAULT_NONE;
}

// 错误处理
void protection_fault(tProtectionManager *pm, eFault fault) {
    if (pm->fault != FAULT_NONE)
        return;
    pm->fault = fault;

    PM_error_Callback(pm);
}

// 保护服务主循环
void protection_loop_task(tProtectionManager *pm, tMotorCtrl *motor,
                          const tObsData *data) {
    status_fb_loop_task(pm);
    if (pm->fault != FAULT_NONE)
        return;

    // --- 错误监测

    // 过压
    if (*data->vbus > pm->max_voltage) {
        protection_fault(pm, FAULT_OVERVOLTAGE);
    }

    // 电流过大
    if (tolerance_check(
        *data->current_fb, pm->max_current, -pm->max_current)) {
        protection_fault(pm, FAULT_OVERCURRENT);
    }

    // 温度过高
    if (*data->driver_temp > pm->max_temp) {
        protection_fault(pm, FAULT_OVERTEMP);
    }

    // 使能之后保护
    if (pm->state == STATE_RUNNING) {
        // 电压异常
        if (*data->vbus > pm->max_voltage) {
            protection_fault(pm, FAULT_OVERVOLTAGE);
        }
    }
}


// TODO:添加日志功能
