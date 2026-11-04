#ifndef __XFOC_MAIN_H
#define __XFOC_MAIN_H

#include <stdbool.h>

#include "cmd_list.h"
#include "data_list.h"
#include "parameter_list.h"
#include "slot_con.h"

#include "control.h"
#include "filter.h"
#include "obs.h"
#include "svpwm.h"
#include "trajectory.h"

typedef struct {
    eMotorState mode;

    bool ol_vel_ctrl; // 开启开环速度控制
    bool tau_ctrl;    // 开启电流控制
    bool fw_ctrl;     // 弱磁控制
    bool pvt_ctrl;    // 开启pvt模式
    float mit_kp;
    float mit_kd;
} tCmdCfg;

typedef struct {
    eMotorState mode;
    struct {
        float value; // 角度或者速度
        float duty;  // 占空比
    } openloop;
    struct {
        float value; // 拖动电流/转矩 N/m
    } current_ctrl;

    struct {
        float vel; // 速度 (rad/s)
    } vel_pid_ctrl;

    struct {
        float pos; // 位置 (rad)
    } pos_pid_ctrl;

    struct {
        float pos;    // 位置 (rad)
        float vel;    // pvt vel
        float tau_ff; // 转矩前馈
    } mit_ctrl;

} tCmdValue;

// 主要核心
typedef struct {
    eMotorState state; // 状态

    tParameter *param;                     // 参数
    tCtrl ctrl;                            // 控制器
    tObs obs;                              // 观测器
    tSvpwm svpwm;                          // 调制器
    tFirstOrderLagFilter cf_u, cf_v, cf_w; // 电流滤波器
    tTraj traj;                            // 内置轨迹规划器
    struct {
        volatile bool cur_enable;  // 电流使能标记
        volatile bool ol_vel_ctrl; // 开启开环速度控制
        volatile bool tau_ctrl;    // 开启力矩控制
        volatile bool fw_ctrl;     // 弱磁控制
        volatile bool pvt_ctrl;    // 开启pvt模式
    } cfg;
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
        float udc, vmax, temp; // 母线电压 V  最大相电压 驱动温度 C
        float motor_temp;      // 电机温度 C，由电机温度采样链路更新

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
} tXFocMotor;

extern tXFocMotor motor;

void motor_init(tParameter *param, float t_hl, float t_ml, float t_ll);
void motor_reset(void);
void current_filter_init(float cfalpha);
void motor_disable(void);
void motor_handle_fault(void);
bool motor_cmd_cfg(tCmdCfg *cfg);
bool motor_cmd_set(tCmdValue *cmd);
bool motor_set_pos_zero(void);
bool motor_set_pos_limit(void);

void motor_mainloop_tasks(void);
void obs_foc_task(float ts);
void traj_task(float ts);
void vl_pid_mit_task(float ts);
void field_weakening_task(float ts);
void pl_pid_task(float ts);

#endif