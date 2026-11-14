#ifndef __GTAE_DRV_H
#define __GTAE_DRV_H

#include "pwm.h"
#include "sample.h"
// 所有栅极驱动的管理
typedef struct {
    tPwmDrv *pd; // pwm 驱动业务
    tSample *sp; // 采样业务
    float Vbus;
    float Temp;
    uint16_t pwm_arr;
    uint16_t ticU, ticV, ticW;

} tGateDrv;

extern tGateDrv g_gate_drv;

void gd_init(tPwmDrv *pd, tSample *sp);
void gd_loop_task(bool motor_idle);
// 对外接口
void gd_poweron(bool on);
void gd_enable(bool en);
void gd_output(float Du, float Dv, float Dw);
void gd_get_current(float is[3]);
void gd_calibration(uint8_t sec);
#endif