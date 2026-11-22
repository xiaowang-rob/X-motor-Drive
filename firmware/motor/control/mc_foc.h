#ifndef MC_FOC_H
#define MC_FOC_H

#include "core/mc_types.h"

typedef struct {
    float is[3];
    tAb i_ab;
    tDq i_dq;
    tPwm pwm;
    float sin, cos;            /* 本周期 Park / 反 Park 共用 */
} tMcFoc;

void foc_init(tMcFoc *foc);
bool foc_current(tMcFoc *foc, const tSample *sample, float alpha);
void foc_park(tMcFoc *foc, float angle);
/* u 为已限幅的归一化 dq 电压；复用 project 的 sin/cos。 */
void foc_pwm(tMcFoc *foc, tDq u);

#endif
