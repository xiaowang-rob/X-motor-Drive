#ifndef __CORE_H
#define __CORE_H

#include <stdbool.h>

#include "protocol.h"

#include "pid.h"

// 主要核心
typedef struct
{
    eCoreState state; // 状态

    volatile bool enable;    // 使能标记
    volatile bool ov_enable; // 开环电压使能标记

    volatile bool obs_enable; // 观测器使能标记
    volatile bool enc_enable; // 编码器使能标记

    eObsList obs;        // 观测器
    eCtrlMode ctrl_mode; // 控制模式

    struct
    {
        float vel;      // 速度 (rad/s)
        float pos;      // 位置 (rad)
        float tau_ff;   // 转矩前馈
        float vel_elec; // 电角速度 rad/s
        float iq, id;
        float tau; // 转矩 N/m
    } ref;

    struct
    {
        float udc, vmax, temp; // 母线电压 V  最大相电压 温度 C

        float ims[3];        // 三相原始电流（未滤波） A
        float is[3];         // 三相电流 A
        float ialpha, ibeta; // ab电流 A
        float iq, id;        // dq电流 A

        float theta_elec;   // 电角度 rad
        float sin_e, cos_e; // 电角度 sin cos

        float ud, uq;        // qd电压 V
        float ualpha, ubeta; // foc 输出电压 V

        float tau; // 转矩 N/m

        float theta_enc;  // 编码器角度 rad
        float theta_mech; // 机械角度 rad

        float vel; // 转速 rad/s
        float pos; // 位置 rad
    } fb;
} tCore;

bool core_init(void);

#endif