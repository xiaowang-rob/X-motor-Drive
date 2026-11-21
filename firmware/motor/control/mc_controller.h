#ifndef __MC_CONTROLLER_H
#define __MC_CONTROLLER_H

#include "filter.h"
#include "xmath.h"

#include "mit.h"
#include "pid.h"
#include "trajectory.h"

#include "data_list.h"
#include "parameter_list.h"

typedef struct {
    eMotorState mode;

    tTraj traj; // 内置轨迹规划器
    tPI PI_vel;
    tPID PID_pos;
    tMIT mit;

    volatile bool init_flag; // 初始化标志

    volatile bool ov_enable;  // 开环速度使能
    volatile bool tau_enable; // 力矩输入使能
    volatile bool pvt_enable; // pvt模式使能

    struct {
        float vel;          // 速度 (rad/s)
        float pos;          // 位置 (rad)
        float id, iq;       // dq电流
        float tau;          // 转矩 N/m
        float tau_ff;       // 转矩前馈
        float theta_elec;   // 电角度rad
        float sin_e, cos_e; // 电角度 sin cos
        float vel_elec;     // 电角速度 rad/s
    } ref;

    struct {
        float theta_rotor; // 转子角度
        float theta_elec;  // 电角度 rad
        float vel;         // 转速 rad/s
        float pos;         // 位置 rad
    } fb;

} tCtrl;

void ctrl_reset(tCtrl *con);
void ctrl_cmd_process(tCtrl *con, tParameter *param, float ts);
void ctrl_config(tCtrl *con, tParameter *param, float ts);
void ctrl_update(tCtrl *con, tParameter *param, float ts);
#endif