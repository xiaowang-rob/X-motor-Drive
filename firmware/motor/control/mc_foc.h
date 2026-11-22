#ifndef __MC_FOC_H
#define __MC_FOC_H

#include "filter.h"

#include "pid.h"

#include "parameter_list.h"

typedef struct {
    tFirstOrderLagFilter cf_u, cf_v, cf_w; // 电流滤波器
    tPI PI_iq;
    tPI PI_id;
    tPI PI_fw;

    volatile bool to_enable; // 角度开环使能
    volatile bool cl_enable; // 电流闭环使能
    volatile bool fw_enable; // 弱磁开关

    struct {
        float id, iq;       // dq 电流
        float theta_e;      // 电角度
        float sin_e, cos_e; // 电角度 sin cos
        float vel_elec;     // 电角速度 rad/s
        uint8_t sec;        // 扇区
    } ref;

    struct {
        float is[3];  // 三相电流
        float ia, ib; // alpha beta 电流
        float id, iq; // dq 电流
    } fb;

    struct {
        float duty;
        float duty_d, duty_q;
        float duty_a, duty_b;
        float dutyU, dutyV, dutyW;
    } out;

} tMcFoc;

void mc_foc_init(tMcFoc *foc, tParameter *param, float ts);
void mc_foc_reset(tMcFoc *foc);
void mc_foc_current_process(tMcFoc *foc, float is_in[3]);
void mc_foc_update(tMcFoc *foc, float ts, float fb_theta_e);
void mc_foc_svpwm_output(tMcFoc *foc);

// 弱磁控制
static inline void field_weakening_update(tMcFoc *foc) {

    foc->out.duty = foc->out.duty_q;
    foc->ref.id = 0.0f;
    if (!foc->fw_enable)
        return;

    if (foc->out.duty_q < 0.9f)
        return;

    foc->out.duty = XSQRTF(foc->out.duty_d * foc->out.duty_d +
                           foc->out.duty_q * foc->out.duty_q);
    foc->ref.id = pi_update(&foc->PI_fw, 1, foc->out.duty);
}

#endif