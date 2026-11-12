#ifndef __MOTOR_CTRL_H
#define __MOTOR_CTRL_H

#include <stdbool.h>

#include "cmd_list.h"
#include "data_list.h"
#include "parameter_list.h"

#include "control.h"
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

// 位置记录
typedef struct {
    int32_t num_turns; // 圈数累积

    float last_angle; // 上一个机械角度
    float zero_angle; // 位置机械零点
    float max_pos;    // 位置机械最大值
    float min_pos;    // 位置机械最小值

} tPosAcc;

typedef struct {
    void (*pwm_enable)(bool en);
    void (*pwm_out)(float Du, float Dv, float Dw);
} tMotorOps;
// 主要核心
typedef struct {
    const tMotorOps *ops;
    eMotorState state; // 状态

    bool enc_on; // 有感
    bool obs_on; // 无感

    tParameter *param; // 参数
    tPosAcc pacc;      // 位置累积
    tCtrl ctrl;        // 控制器
    tTraj traj;        // 内置轨迹规划器

    struct {
        volatile bool cur_enable;  // 电流闭环使能标记
        volatile bool ol_vel_ctrl; // 开启开环速度控制
        volatile bool tau_ctrl;    // 开启力矩控制
        volatile bool fw_ctrl;     // 弱磁控制
        volatile bool pvt_ctrl;    // 开启pvt模式
    } cfg;
    struct {
        float vel;                 // 速度 (rad/s)
        float pos;                 // 位置 (rad)
        float tau_ff;              // 转矩前馈
        float theta_elec;          // 电角度rad
        float sin_e, cos_e;        // 电角度 sin cos
        float vel_elec;            // 电角速度 rad/s
        uint8_t sec;               // 扇区
        float id, iq;              // dq电流
        float tau;                 // 转矩 N/m
        float dutyd, dutyq;        // qd 占空比
        float dutya, dutyb;        // ab 占空比
        float dutyU, dutyV, dutyW; // 三相占空比输出
    } ref;

    struct {
        float motor_temp; // 电机温度 C，由电机温度采样链路更新

        float ims[3];        // 三相原始电流（未滤波） A
        float is[3];         // 三相电流 A
        float ialpha, ibeta; // ab电流 A
        float iq, id;        // dq电流 A
        float tau;           // 转矩 N/m

        float theta_rotor; // 转子角度
        float theta_elec;  // 电角度 rad
        float vel;         // 转速 rad/s
        float pos;         // 位置 rad
    } fb;

} tMotorCtrl;

extern tMotorCtrl g_motor;

void motor_init(tParameter *param, float t_hl, float t_ml, float t_ll);
void motor_reset(void);
void motor_disable(void);
void motor_handle_fault(void);
bool motor_cmd_cfg(tCmdCfg *cfg);
bool motor_cmd_set(tCmdValue *cmd);
void motor_set_pos_zero(void);
void motor_set_pos_limit(void);

void motor_loop_task(void);

void motor_foc_tim_task(float ts);
void motor_traj_tim_task(float ts);
void motor_vl_pid_mit_tim_task(float ts);
void motor_field_weakening_tim_task(float ts);
void motor_pl_pid_tim_task(float ts);

#endif