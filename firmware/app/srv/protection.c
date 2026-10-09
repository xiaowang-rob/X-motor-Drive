
#include "protection.h"

#include "bsp_cfg.h"

#include "protocol.h"

tProtectionManager g_pro_manager;
// 容忍度检测
static bool _tolerance_check(float value, float max_value, float min_value)
{
    if (value > max_value * g_pro_manager.tolerance_limit || value < min_value / g_pro_manager.tolerance_limit)
        return true;
    return false;
}
// 保护程序 初始化
void pro_manager_init(tParameter *param)
{

    g_pro_manager.fault_flag = false;
    g_pro_manager.warning_flag = false;
    g_pro_manager.max_current = param->limit_current;
    g_pro_manager.max_vel = param->limit_vel;
    g_pro_manager.min_position = param->limit_position_min;
    g_pro_manager.max_position = param->limit_position_max;
    g_pro_manager.tolerance_time_ms = param->tolerance_time;
    if (param->tolerance_limit < 0.6f)
    {
        param->tolerance_limit = 0.6f;
    }
    g_pro_manager.tolerance_limit = param->tolerance_limit;
}

// 设置保护程序的限位位置
void pro_set_limit_position(float min_position, float max_position)
{
    g_pro_manager.min_position = min_position;
    g_pro_manager.max_position = max_position;
}
// 保护程序复位
void pro_manager_reset(void)
{
    g_pro_manager.fault_flag = false;
    g_pro_manager.warning_flag = false;
}

// 错误处理
void pm_handle_fault(eFault fault)
{
    g_pro_manager.fault_flag = true;
    core_enter_fault(fault);
}

void pm_handle_warning(eWarning warning)
{
    g_pro_manager.warning_flag = true;
    core_enter_warning(warning);
}

// 保护服务主循环
void pro_manager_main_loop(tCore *core)
{

    if (g_pro_manager.fault_flag)
        return;

    // --- 错误监测

    // 过压
    if (core->fb.udc > MAX_VOLTAGE)
    {
        pm_handle_fault(FAULT_OVERVOLTAGE);
    }

    // 4.CAN通讯异常
    if (!core->com_running)
    {
        pm_handle_warning(WARNING_BUSCOM_RUN);
    }

    //  4编码器状态检测
    if (!core->obs_running)
    {
        pm_handle_warning(WARNING_OBS_RUN);
    }
    // 3.电流过大
    if (FABSF(core->fb.is[0]) > MAX_CURRENT || FABSF(core->fb.is[1]) > MAX_CURRENT || FABSF(core->fb.is[2]) > MAX_CURRENT ||
        _tolerance_check(core->fb.iq, g_pro_manager.max_current, -g_pro_manager.max_current))
    {
        pm_handle_fault(FAULT_OVERCURRENT);
    }

    // 警告：
    // 1温度过高
    if (core->fb.temp > MAX_TEMPERATURE)
    {
        pm_handle_warning(WARNING_OVERTEMP);
    }

    // 使能之后保护
    if (core->enable)
    {
        // 2.电压异常
        if (_tolerance_check(core->fb.udc, MAX_VOLTAGE, MIN_VOLTAGE))
        {
            if (core->fb.udc > MAX_VOLTAGE)
            {
                pm_handle_fault(FAULT_OVERVOLTAGE);
            }
            if (core->fb.udc < MIN_VOLTAGE)
            {
                pm_handle_fault(FAULT_UNDERVOLTAGE);
            }
        }

        // 2 速度检测
        if (_tolerance_check(core->fb.vel, g_pro_manager.max_vel, -g_pro_manager.max_vel))
        {
            pm_handle_warning(WARNING_OVERSPEED);
        }
        // 3 位置检测
        if (core->state == PID_POSITION || core->state == MIT_MODE)
        {
            if (_tolerance_check(core->fb.pos, g_pro_manager.max_position, g_pro_manager.max_position))
            {
                pm_handle_warning(WARNING_POSITION_LIMIT);
            }
        }
    }

    //   B错误处理--日志模块还得优化
    if (g_pro_manager.fault_flag || g_pro_manager.warning_flag)
    {
        log_data_save(&g_pro_manager);
        // TODO: 添加错误处理
        //  log_data_write();
        // TODO: 添加警告处理
        //  log_data_write();
    }
}
