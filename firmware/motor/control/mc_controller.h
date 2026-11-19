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

    tPI PI_vel;
    tPID PID_pos;
    tMIT mit;

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

// pid速度环更新 返回iq
static inline float pid_vl_update(tCtrl *con) {
    return pi_update(&con->PI_vel, con->ref.vel, con->fb.vel);
}

// pid位置环更新 pvc下返回vel pc下返回iq
static inline float pid_pl_update(tCtrl *con) {
    return pid_update(&con->PID_pos, con->ref.pos, con->fb.pos);
}

// mit 控制更新 返回转矩
static inline float mit_con_update(tCtrl *con, float tau_ff, float pos_ref,
                                   float pos_fb, float vel_ref, float vel_fb) {
    return mit_update(&con->mit, tau_ff, pos_ref, pos_fb, vel_ref, vel_fb);
} // pid速度环更新 返回iq
static inline float pid_vl_update(tCtrl *con) {
    return pi_update(&con->PI_vel, con->ref.vel, con->fb.vel);
}

// pid位置环更新 pvc下返回vel pc下返回iq
static inline float pid_pl_update(tCtrl *con) {
    return pid_update(&con->PID_pos, con->ref.pos, con->fb.pos);
}

// mit 控制更新 返回转矩
static inline float mit_con_update(tCtrl *con, float tau_ff, float pos_ref,
                                   float pos_fb, float vel_ref, float vel_fb) {
    return mit_update(&con->mit, tau_ff, pos_ref, pos_fb, vel_ref, vel_fb);
}

#endif