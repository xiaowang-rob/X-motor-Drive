#ifndef __FOC_H
#define __FOC_H

#include "protocol.h"
#include "pid.h"
#include "mit.h"
#include "svpwm.h"
#include "filter.h"

#include "parameters.h"
#include "trajectory.h"

typedef struct
{
    float vel_elec; // 电角速度 rad/s
    float iq, id;
    float tua;
} tFOCtarget;

typedef struct
{
    float imu, imv, imw; // 三相原始电流（未滤波） A
    float iu, iv, iw;    // 三相电流 A
    float ialpha, ibeta; // ab电流 A
    float iq, id;        // dq电流 A

    float theta_elec;   // 电角度 rad
    float sin_e, cos_e; // 电角度 sin cos

    float ud, uq;        // qd电压 V
    float ualpha, ubeta; // foc 输出电压 V

    float tau; // 转矩 N/m
} tFOCval;

typedef struct
{

    tFirstOrderLagFilter cf_u, cf_v, cf_w; // 电流滤波器
    tPI PI_iq;
    tPI PI_id;

    tFOCtarget tag; // 目标值
    tFOCval val;    // 反馈

} tFOC;

bool foc_init(tFOC *foc, tParameter *param, float t_cl, float vmax);
bool foc_reset(tFOC *foc);
void foc_process(tFOC *foc, uint8_t sec);
void foc_update(tFOC *foc);
void foc_set_target(tFOC *foc, tFOCtarget tag);

#endif // __FOC_H