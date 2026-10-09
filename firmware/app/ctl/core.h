#ifndef __CORE_H
#define __CORE_H

#include <stdbool.h>

#include "protocol.h"

typedef struct
{
    eCoreState mode;
    float vel;      // 速度 (rad/s)
    float pos;      // 位置 (rad)
    float tp;       // pvt模式下时刻
    float tau_ff;   // 转矩前馈
    float vel_elec; // 电角速度 rad/s
    float id;       // 电枢电流 A
    float iq;       // 拖动电流 A
    float tau;      // 转矩 N/m
} tCmd;

// 主要核心
typedef struct
{
    eCoreState state; // 状态
    eFault fault;     // 故障
    eWarning warning; // 警告

    volatile bool enable; // 使能标记

    volatile bool ov_enable; // 开环电压使能标记
    volatile bool ot_enable; // 开环角度使能标记

    bool obs_running; // 观测器是否正常运行
    bool com_running; // 通信是否正常运行

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
        float ud, uq;        // qd电压 V
        float ualpha, ubeta; // foc 输出电压 V

        float tau; // 转矩 N/m

        float theta_elec;   // 电角度 rad
        float sin_e, cos_e; // 电角度 sin cos

        float vel; // 转速 rad/s
        float pos; // 位置 rad
    } fb;
} tCore;

void core_init(void);
void core_reset(void);
void current_filter_init(float cfalpha);
void core_disable(void);
bool core_cmd_set(tCmd *cmd);
void core_ov_cmd_set(float ud, float uq, float theta_e);
void core_ot_cmd_set_theta(float theta_e);
void core_ot_cmd_set_vel(float vel);

void core_enter_fault(eFault fault);
void core_enter_warning(eWarning warning);

void core_mainloop_tasks(void);

#endif