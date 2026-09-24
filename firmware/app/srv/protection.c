
#include "protection.h"

#include "bsp_cfg.h"
#include "core.h"
#include "foc.h"

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

// 设置保护程序的限位位置
void pro_set_limit_position(float min_position, float max_position)
{
    g_pro_manager.min_position = min_position;
    g_pro_manager.max_position = max_position;
}
// 保护程序复位
void pro_manager_reset()
{
    g_pro_manager.fault_flag = false;
    g_pro_manager.warning_flag = false;
    g_pro_manager.fault = FAULT_NONE;
    g_pro_manager.warning = WARNING_NONE;
}

// 保护服务主循环
void pro_manager_main_loop(tCore *core, tFOCval *foc_val)
{

    if (g_pro_manager.fault_flag)
        return;

    // --- 错误监测

    // 过压不可取
    if (core->val.udc > MAX_VOLTAGE)
    {
        g_pro_manager.fault = FAULT_OVERVOLTAGE;
        g_pro_manager.fault_flag = true;
    }

    // 4.CAN通讯异常
    if (g_can.dstate != DEV_ONLINE && g_can.dstate != DEV_RUNNING)
    {
        if (g_can.dstate == DEV_RUN_ERROR)
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
    if (core->enc_enable)
    {
        // TODO：后续状态只监测运行错误   初始化错误在初始化的时候直接返回
        //  板载编码器状态检测
        if (g_enc_int.dstate != DEV_ONLINE && g_enc_int.dstate != DEV_RUNNING)
        {
            g_pro_manager.warning_flag = true;
            if (g_enc_int.dstate == DEV_RUN_ERROR)

                g_pro_manager.warning = WARNING_ENCODER_COMM_ERR;
            else
                g_pro_manager.warning = WARNING_ENCODER_OFFLINE;
        }
    }
    // 3.电流过大
    if (FABSF(foc_val->iu) > MAX_CURRENT || FABSF(foc_val->iv) > MAX_CURRENT || FABSF(foc_val->iw) > MAX_CURRENT ||
        _tolerance_check(foc_val->iq, g_pro_manager.max_current, -g_pro_manager.max_current))
    {
        g_pro_manager.fault = FAULT_OVERCURRENT;
        g_pro_manager.fault_flag = true;
    }

    // 警告：
    // 1温度过高
    if (core->val.temp > MAX_TEMPERATURE)
    {
        g_pro_manager.warning = WARNING_OVERTEMP;
        g_pro_manager.warning_flag = true;
    }

    // 使能之后保护
    if (core->enable)
    {
        // 1.整定
        // TODO:整定模块检测
        if (tune_get_fault() != FAULT_NONE)
        {
            g_pro_manager.fault = tune_get_fault();
            g_pro_manager.fault_flag = true;
        }
        // 2.电压异常
        if (_tolerance_check(core->val.udc, MAX_VOLTAGE, MIN_VOLTAGE))
        {
            if (core->val.udc > MAX_VOLTAGE)
            {
                g_pro_manager.fault = FAULT_OVERVOLTAGE;
                g_pro_manager.fault_flag = true;
            }
            if (core->val.udc < MIN_VOLTAGE)
            {
                g_pro_manager.fault = FAULT_UNDERVOLTAGE;
                g_pro_manager.fault_flag = true;
            }
        }

        // 2 速度检测
        if (_tolerance_check(core->val.vel, g_pro_manager.max_vel, -g_pro_manager.max_vel))
        {
            g_pro_manager.warning = WARNING_OVERSPEED;
            g_pro_manager.warning_flag = true;
        }
        // 3位置检测 位置模式下监测
        if (core->ctrl_mode == PID_POSITION || core->ctrl_mode == MIT_MODE)
        {
            if (_tolerance_check(core->val.pos, g_pro_manager.max_position, g_pro_manager.min_position))
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
