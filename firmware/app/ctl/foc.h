#ifndef __FOC_H
#define __FOC_H

#include "protocol.h"
#include "pid.h"
#include "mit.h"
#include "filter.h"

#include "parameters.h"

typedef struct
{
    tPI PI_iq;
    tPI PI_id;
    tPI PI_weakmag;
    tPI PI_vel;
    tPID PID_pos;
    tMIT mit;
} tFOC;

void foc_init(tFOC *foc, tParameter *param,
              float t_cl, float t_vl, float t_pl, float vmax);
void foc_reset(tFOC *foc);

// 电流内环更新 pi输出dq电压
static inline void foc_cl_update(tFOC *foc, float iq_ref, float iq_fb, float id_ref, float id_fb, float *uq, float *ud)
{
    *uq = pi_update(&foc->PI_iq, iq_ref, iq_fb);
    *ud = pi_update(&foc->PI_id, id_ref, id_fb);
}

// pid速度环更新
static inline void foc_pidvl_update(tFOC *foc, float vel_ref, float vel_fb, float *iq)
{
    *iq = pi_update(&foc->PI_vel, vel_ref, vel_fb);
}

// 弱磁控制
static inline void foc_weakmag_update(tFOC *foc, float vout_ref, float vout_fb, float *id)
{
    *id = pi_update(&foc->PI_weakmag, vout_ref, vout_fb);
}

// pid位置环更新
static inline void foc_pidpl_update(tFOC *foc, float pos_ref, float pos_fb, float *vel)
{
    *vel = pid_update(&foc->PID_pos, pos_ref, pos_fb);
}

// mit 控制更新
static inline void foc_mit_update(tFOC *foc, float tau_ff, float pos_ref, float pos_fb, float vel_ref, float vel_fb, float *tau)
{
    *tau = mit_update(&foc->mit, tau_ff, pos_ref, pos_fb, vel_ref, vel_fb);
}

#endif // __FOC_H