#ifndef __CORE_H
#define __CORE_H

#include <stdbool.h>

#include "pid.h"
#include "mit.h"

// pid 目标值
typedef struct
{
    float vel;
    float pos;
} tPIDtarget;

// mit 目标值
typedef struct
{
    float vel;    // 速度 (rad/s)
    float pos;    // 位置 (rad)
    float tau_ff; // 转矩前馈
} tMITtarget;

// 位置记录
typedef struct
{
    int32_t num_turns; // 圈数累积

    float last_angle; // 上一个机械角度
    float zero_angle; // 位置机械零点
    float max_pos;    // 位置机械最大值
    float min_pos;    // 位置机械最小值

} tPosAcc;

// 核心数据
typedef struct
{
    float udc, vmax, temp; // 母线电压 V  最大相电压 温度 C

    float theta_enc;  // 编码器角度 rad
    float theta_mech; // 机械角度 rad

    float vel; // 转速 rad/s
    float pos; // 位置 rad
} tCoreVal;
// 主要核心
typedef struct
{
    eState state; // 状态

    volatile bool enable;     // 使能标记
    volatile bool obs_enable; // 观测器使能标记
    volatile bool enc_enable; // 编码器使能标记

    eObsMode obs_mode; // 观测器模式
    eRunMode run_mode; // 运行模式

    tPI PI_weakmag;
    tPI PI_vel;
    tPID PID_pos;
    tMIT mit;

    tPIDtarget pidtag; // PID 目标值
    tMITtarget mittag; // MIT 目标值

    tPosAcc pacc; // 位置记录

    tCoreVal val; // 反馈数据
} tCore;
#endif