#include "parameter_manager.h"
#include <stddef.h>
#include "string.h"
#include "foc_main.h"
#include "math_fast.h"
#include "protection_manager.h"
#include "can_port.h"
#include "usr_config.h"

tParameter g_Param;

// param 描述符表
#define PARAM_ENTRY(id, field) [id] = {.offset = offsetof(tParameter, field), .size = sizeof(((tParameter *)0)->field)}

const tParamEntry g_param_table[] = {
    PARAM_ENTRY(ENCODER_CHIP, encoder_chip),
    PARAM_ENTRY(SENSOR_MODE, sensor_mode),
    PARAM_ENTRY(RUN_MODE, run_mode),
    PARAM_ENTRY(CAN_MODE, sw_canqueue),
    PARAM_ENTRY(VAGUE_PID_MODE, sw_vague_pid),
    PARAM_ENTRY(PVT_MODE, sw_pvt),
    PARAM_ENTRY(TRAJ_TYPE, traj_type),
    PARAM_ENTRY(MOTOR_POLEPAIRS, motor_polepairs),
    PARAM_ENTRY(CAN_ID, can_id),
    PARAM_ENTRY(THETA_OFFSET, theta_offset),
    PARAM_ENTRY(MOTOR_KV, motor_kv),
    PARAM_ENTRY(MOTOR_RS, motor_rs),
    PARAM_ENTRY(MOTOR_Ld, motor_ld),
    PARAM_ENTRY(MOTOR_Lq, motor_lq),
    PARAM_ENTRY(MOTOR_PSIF, motor_psif),
    PARAM_ENTRY(MOTOR_KE, motor_ke),
    PARAM_ENTRY(MOTOR_J, motor_j),
    PARAM_ENTRY(MOTOR_B, motor_b),
    PARAM_ENTRY(KP_SPEED, kp_speed),
    PARAM_ENTRY(KI_SPEED, ki_speed),
    PARAM_ENTRY(KP_POSITION, kp_position),
    PARAM_ENTRY(KI_POSITION, ki_position),
    PARAM_ENTRY(KD_POSITION, kd_position),
    PARAM_ENTRY(ALPHA_POSITION, alpha_position),
    PARAM_ENTRY(MIT_KP, kp_MIT),
    PARAM_ENTRY(MIT_KD, kd_MIT),
    PARAM_ENTRY(MIT_TMAX, tmax_MIT),
    PARAM_ENTRY(TUNE_CURRENT, tune_current),
    PARAM_ENTRY(LIMIT_CURRENT, limit_current),
    PARAM_ENTRY(LIMIT_VELOCITY, limit_vel),
    PARAM_ENTRY(LIMIT_POSITION_MIN, limit_position_min),
    PARAM_ENTRY(LIMIT_POSITION_MAX, limit_position_max),
    PARAM_ENTRY(TOLERANCE_TIME, tolerance_time),
    PARAM_ENTRY(TOLERANCE_LIMIT, tolerance_limit),
    PARAM_ENTRY(TRAJ_LIMIT_D1, traj_limit_d1),
    PARAM_ENTRY(TRAJ_LIMIT_D2, traj_limit_d2),
    PARAM_ENTRY(TRAJ_LIMIT_D3, traj_limit_d3),
    PARAM_ENTRY(TRAJ_TOLERANCE, tolerance),
};

void param_set(eParameter para, u8 *value)
{
    if (para >= PARAM_NUM)
    { // 参数应用
        comm_write_can_config(g_Param.can_id, g_Param.sw_canqueue);
        pro_manager_config(&g_Param);
        foc_core_init(&g_Param);
        return;
    }
    u8 *dst = (u8 *)&g_Param + g_param_table[para].offset;
    memcpy(dst, value, g_param_table[para].size);
}

void param_get(eParameter para, u8 *value, u8 *len)
{
    if (para >= PARAM_NUM)
    {
        *len = 0;
        return;
    }
    u8 *src = (u8 *)&g_Param + g_param_table[para].offset;
    memcpy(value, src, g_param_table[para].size);
    *len = g_param_table[para].size;
}

bool param_save()
{
    bsp_erase_param();
    return bsp_write_param((u8 *)&g_Param, sizeof(g_Param));
}
bool param_init()
{
    // 先清零，避免 Flash 未写入的字段为 NaN/垃圾值
    memset(&g_Param, 0, sizeof(g_Param));

    if (false == bsp_read_param((u8 *)&g_Param, sizeof(g_Param)))
        return false;
    if (g_Param.none_flag != 0x0f)
    { // flash中没有参数，初始化默认值
        g_Param.none_flag = 0x0f;
        g_Param.motor_polepairs = 7;
        g_Param.can_id = 1;
        g_Param.motor_rs = 0.07f;
        g_Param.motor_ld = 45e-6f;
        g_Param.motor_lq = 49e-6f;
        g_Param.motor_psif = 0.00035f;
        g_Param.motor_ke = 0.0025f;
        g_Param.motor_j = 1e-4f;
        g_Param.motor_b = 1e-3f;
        g_Param.kp_speed = 3;
        g_Param.ki_speed = 20;
        g_Param.kp_position = 0.5f;
        g_Param.ki_position = 0.05f;
        g_Param.kd_position = 0.005f;
        g_Param.alpha_position = 0.15f;
        g_Param.tune_current = 1.5f;
        g_Param.limit_current = 30;
        g_Param.limit_vel = 209.44f;
        g_Param.limit_position_min = -15000;
        g_Param.limit_position_max = 15000;
        g_Param.tolerance_time = 1;
        g_Param.tolerance_limit = 1.1f;
        g_Param.traj_limit_d1 = 1000;
        g_Param.traj_limit_d2 = 1000;
        g_Param.traj_limit_d3 = 1000;
        g_Param.tolerance = 0.1f;
        g_Param.kp_Q = 1.1f;
        g_Param.ki_Q = 1.1f;
        g_Param.kp_D = 1.1f;
        g_Param.ki_D = 600;
        g_Param.cur_filter_alpha = 0.4f;
        g_Param.speed_filter_alpha = 0.08f;
        g_Param.adc_U_zero_offset = 2048;
        g_Param.adc_V_zero_offset = 2048;
        g_Param.adc_W_zero_offset = 2048;
        param_save();
    }
    // 修复 NaN：Flash 中有数据但尾部字段未被上位机写入
    else
    {
        bool need_save = false;
        if (g_Param.speed_filter_alpha != g_Param.speed_filter_alpha ||
            g_Param.speed_filter_alpha < 0.01f || g_Param.speed_filter_alpha > 1.0f)
        {
            g_Param.speed_filter_alpha = 0.08f;
            need_save = true;
        }
        if (g_Param.adc_U_zero_offset != g_Param.adc_U_zero_offset)
        {
            g_Param.adc_U_zero_offset = 0;
            g_Param.adc_V_zero_offset = 0;
            g_Param.adc_W_zero_offset = 0;
            need_save = true;
        }
        if (need_save)
            param_save();
    }
    return true;
}

bool param_erase()
{
    return bsp_erase_param();
}
