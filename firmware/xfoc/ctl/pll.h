#ifndef __PLL_H
#define __PLL_H

typedef struct
{
    float kp;
    float ki;
    float integ_limit;
    float vel_limit;

    float theta_delta;
    float theta;
    float vel;
    float integ;
} tPLL;

void pll_init(tPLL *pll, float kp, float ki, float integ_limit, float vel_limit);

void pll_update(tPLL *pll, float theta, float dt);

#endif