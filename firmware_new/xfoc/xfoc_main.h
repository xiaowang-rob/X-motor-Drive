#ifndef __XFOC_MAIN_H
#define __XFOC_MAIN_H

#include <stdbool.h>

#include "cmd_list.h"
#include "data_list.h"

typedef struct {
    eCoreState mode;
    struct {
        bool vel_ctrl; // 开启速度控制
        float value;   // 角度或者速度
        float duty;    // 占空比
    } openloop;
    struct {
        bool tau_ctrl; // 开启电流控制
        float value;   // 拖动电流/转矩 N/m
    } current_ctrl;

    struct {
        float vel; // 速度 (rad/s)
    } vel_pid_ctrl;

    struct {
        float pos; // 位置 (rad)
    } pos_pid_ctrl;

    struct {
        float kp;     // 刚度
        float kd;     // 阻尼
        float pos;    // 位置 (rad)
        float vel;    // pvt vel
        float tau_ff; // 转矩前馈
    } mit_ctrl;

} tCmdValue;

// 主要核心
typedef struct {
    eCoreState state; // 状态
    eFault fault;     // 故障
    eWarning warning; // 警告

    volatile bool enable; // 使能标记

    volatile bool oc_enable; // 电流开环使能标记

    bool obs_running; // 观测器是否正常运行
    bool com_running; // 通信是否正常运行

    struct {
        float vel;        // 速度 (rad/s)
        float pos;        // 位置 (rad)
        float tau_ff;     // 转矩前馈
        float theta_elec; // 电角度rad
        float vel_elec;   // 电角速度 rad/s
        float duty;       // 占空比
        float iq, id;
        float tau; // 转矩 N/m
    } ref;

    struct {
        float udc, vmax, temp; // 母线电压 V  最大相电压 温度 C

        float ims[3];        // 三相原始电流（未滤波） A
        float is[3];         // 三相电流 A
        float ialpha, ibeta; // ab电流 A
        float iq, id;        // dq电流 A
        float ud, uq;        // qd电压 V
        float ualpha, ubeta; // foc 输出电压 V

        float tau; // 转矩 N/m

        float theta_rotor;  // 转子角度
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
void core_cmd_set(tCmdValue *cmd);
void core_set_pos_zero(void);
void core_set_pos_limit(void);
void core_enter_fault(eFault fault);
void core_enter_warning(eWarning warning);

void core_mainloop_tasks(void);

// xfoc 启动
bool xfoc_start(void);

#endif