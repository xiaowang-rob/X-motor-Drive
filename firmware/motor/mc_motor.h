#ifndef __MC_MOTOR_H
#define __MC_MOTOR_H

#include <stdbool.h>

#include "cmd_list.h"
#include "data_list.h"
#include "parameter_list.h"

#include "mc_controller.h"
#include "mc_foc.h"

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

typedef struct {
    void (*theta_process)(float ts, float *th_e, float *th_r, float *vel,
                          float *pos);                  // 处理角度信息
    void (*get_current)(float is[3]);                   // 获取采集电流
    float (*get_motor_temp)(void);                      // 获取电机温度
    void (*gate_drv_enable)(bool en);                   // 栅极驱动 使能
    void (*gate_drv_out)(float Du, float Dv, float Dw); // 栅极驱动 输出
    void (*sp_calibration)(uint8_t sec);                // 校准采样点
} tMotorOps;

// 主要核心
typedef struct {
    const tMotorOps *ops;
    eMotorState state; // 状态

    bool enc_on; // 有感
    bool obs_on; // 无感

    tParameter *param; // 参数

    tTraj traj; // 内置轨迹规划器
    tMcFoc foc; // foc
    tCtrl ctrl; // 控制器

    struct {
        volatile bool cur_enable;  // 电流闭环使能标记
        volatile bool ol_vel_ctrl; // 开启开环速度控制
        volatile bool tau_ctrl;    // 开启力矩控制
        volatile bool fw_ctrl;     // 弱磁控制
        volatile bool pvt_ctrl;    // 开启pvt模式
    } cfg;

    struct {
        float motor_temp; // 电机温度 C，由电机温度采样链路更新
        float ims[3];     // 三相原始电流（未滤波） A

        float theta_rotor; // 转子角度
        float vel;         // 转速 rad/s
        float pos;         // 位置 rad
    } fb;

} tMotorCtrl;

extern tMotorCtrl g_motor;

#endif