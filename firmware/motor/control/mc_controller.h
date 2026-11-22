#ifndef MC_CONTROLLER_H
#define MC_CONTROLLER_H

#include "common/pi.h"
#include "common/pid.h"
#include "common/mit.h"
#include "core/mc_config.h"

typedef struct {
    tPID pos;
    tPI vel, id, iq, fw;
    tMIT mit;
    float imax, inv_kt;
    float vref, qref, u2;
    tDq iref, u;
    bool fw_enable;
} tCtrl;

void ctrl_init(tCtrl *c, const tParameter *p, const tMotorTiming *t);
void ctrl_reset(tCtrl *c, const tParameter *p);
/* 指令变化时执行：直接电流/力矩换算、MIT 增益、弱磁切换。 */
void ctrl_cmd(tCtrl *c, const tRef *ref, tMcMode mode, bool fw);
void ctrl_pos(tCtrl *c, const tParameter *p, const tRef *ref, const tRotor *fb);
void ctrl_vel(tCtrl *c, float ref, float fb);
void ctrl_mit(tCtrl *c, const tRef *ref, const tRotor *fb);
/* 已限幅归一化 dq 电压，1 对应 Vbus/sqrt(3)。 */
void ctrl_current(tCtrl *c, tDq fb);

#endif
