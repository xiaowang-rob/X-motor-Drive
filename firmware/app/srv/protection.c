
#include "protection_manager.h"
#include "usr_config.h"
#include "foc_main.h"
#include "log.h"
#include "bsp_adc.h"
#include "tune.h"

tDeviceStatus g_device_status;
tProtectionManager g_pro_manager = {.com_state = &g_com_state, .drive_state = &g_device_status};
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

    g_pro_manager.foc_mode = get_foc_mode_adr();
    g_pro_manager.foc_val = get_foc_val_adr();

    g_pro_manager.fault = FAULT_NONE;
    g_pro_manager.warning = WARNING_NONE;
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

// 保护程序配置参数
void pro_manager_config(tParameter *param)
{
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
// 保护程序 清除故障和警告标志
void pro_manager_clear_flag()
{
    g_pro_manager.fault_flag = false;
    g_pro_manager.warning_flag = false;
    g_pro_manager.fault = FAULT_NONE;
    g_pro_manager.warning = WARNING_NONE;
}

// 保护主循环
void pro_manager_main_loop()
{

    if (g_pro_manager.fault_flag)
        return;

    // 驱动状态--flash一定得是ONLINE
    if (g_pro_manager.drive_state->flash_state != ONLINE)
    {
        g_pro_manager.fault = FAULT_FLASH_OFFLINE;
        g_pro_manager.fault_flag = true;
    }

    // 过压不可取
    if (g_pro_manager.foc_val->udc > MAX_VOLTAGE)
    {
        g_pro_manager.fault = FAULT_OVERVOLTAGE;
        g_pro_manager.fault_flag = true;
    }

    // 4.CAN通讯异常
    if (g_pro_manager.drive_state->can_state != ONLINE && g_pro_manager.drive_state->can_state != RUNNING)
    {
        if (g_pro_manager.drive_state->can_state == RUN_ERROR)
        {
            g_pro_manager.fault = FAULT_CAN_COMM_ERR;
        }
        else
        {
            g_pro_manager.fault = FAULT_CAN_INIT_FAIL;
        }
        g_pro_manager.fault_flag = true;
    }
    //  4编码器状态检测
    if (g_pro_manager.foc_mode->sensor_mode != SENSORLESS_CONTROL)
    { // 有感模式和混合模式启动编码器判断
        if (g_pro_manager.drive_state->encoder_state != ONLINE && g_pro_manager.drive_state->encoder_state != RUNNING)
        {
            g_pro_manager.warning_flag = true;
            if (g_pro_manager.drive_state->encoder_state == RUN_ERROR)

                g_pro_manager.warning = WARNING_ENCODER_COMM_ERR;
            else
                g_pro_manager.warning = WARNING_ENCODER_OFFLINE;
        }
    }
    // 3.电流过大
    if (FABSF(g_pro_manager.foc_val->iu) > MAX_CURRENT || FABSF(g_pro_manager.foc_val->iv) > MAX_CURRENT || FABSF(g_pro_manager.foc_val->iw) > MAX_CURRENT ||
        _tolerance_check(g_pro_manager.foc_val->iq_fb, g_pro_manager.max_current, -g_pro_manager.max_current))
    {
        g_pro_manager.fault = FAULT_OVERCURRENT;
        g_pro_manager.fault_flag = true;
    }

    // 警告：
    // 1温度过高
    if (g_pro_manager.foc_val->temp > MAX_TEMPERATURE)
    {
        g_pro_manager.warning = WARNING_OVERTEMP;
        g_pro_manager.warning_flag = true;
    }

    // 使能之后保护
    if (g_foc.foc_enable)
    {
        // 1.整定
        if (tune_get_fault() != FAULT_NONE)
        {
            g_pro_manager.fault = tune_get_fault();
            g_pro_manager.fault_flag = true;
        }
        // 2.电压异常
        if (_tolerance_check(g_pro_manager.foc_val->udc, MAX_VOLTAGE, MIN_VOLTAGE))
        {
            if (g_pro_manager.foc_val->udc > MAX_VOLTAGE)
            {
                g_pro_manager.fault = FAULT_OVERVOLTAGE;
                g_pro_manager.fault_flag = true;
            }
            if (g_pro_manager.foc_val->udc < MIN_VOLTAGE)
            {
                g_pro_manager.fault = FAULT_UNDERVOLTAGE;
                g_pro_manager.fault_flag = true;
            }
        }

        // 2 速度检测
        if (_tolerance_check(g_pro_manager.foc_val->vel_fb, g_pro_manager.max_vel, -g_pro_manager.max_vel))
        {
            g_pro_manager.warning = WARNING_OVERSPEED;
            g_pro_manager.warning_flag = true;
        }
        // 3位置检测 位置模式下监测
        if (g_pro_manager.foc_mode->run_mode == PID_POSITION || g_pro_manager.foc_mode->run_mode == MIT_POSITION)
        {
            if (_tolerance_check(g_pro_manager.foc_val->pos_fb, g_pro_manager.max_position, g_pro_manager.min_position))
            {
                g_pro_manager.warning = WARNING_POSITION_LIMIT;
                g_pro_manager.warning_flag = true;
            }
        }
    }

    //   B错误处理--日志模块还得优化
    if (g_pro_manager.fault_flag || g_pro_manager.warning_flag)
    {
        log_data_save(&g_pro_manager);
        foc_state_update(FOC_FAULT);
        // TODO: 添加警告处理
        //  log_data_write();
    }
}
