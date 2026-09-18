#include "pll.h"
#include "math_fast.h"

// 默认 PLL 增益与错误判定
#define ENCODER_PLL_KP 80.0f
#define ENCODER_PLL_KI 2000.0f
#define ENCODER_PLL_INTEG_LIMIT 0.1745f // 积分限值  ±10°
#define ENCODER_VEL_PHYS_LIMIT 1046.0f  // rad/s 物理上限（≈10k rpm）

// TODO:20khz pll跟随1khz角度变化 需要1khz的角度突变处理
void pll_init(tPLL *pll, float kp, float ki, float integ_limit, float vel_limit)
{
    pll->kp = kp;
    pll->ki = ki;
    pll->integ_limit = integ_limit;
    pll->vel_limit = vel_limit;
    pll->theta = 0.0f;
    pll->integ = 0.0f;
    pll->vel = 0.0f;
    pll->theta_delta = 0.0f;
}
void pll_update(tPLL *pll, float theta, float dt)
{
    if (!pll || dt <= 0.0f)
        return;

    pll->theta_delta = normalize_angle_pi(theta - pll->theta);

    pll->integ += pll->theta_delta * dt;
    if (pll->integ > pll->integ_limit)
        pll->integ = pll->integ_limit;
    if (pll->integ < -pll->integ_limit)
        pll->integ = -pll->integ_limit;

    float estimated_speed = pll->kp * pll->theta_delta + pll->ki * pll->integ;
    pll->theta += estimated_speed * dt;
    pll->theta = normalize_angle_2pi(pll->theta);
    pll->vel = (FABSF(estimated_speed) < 0.05f) ? 0.0f : estimated_speed;

    // 超物理速度视为失锁，重锁到当前角度
    if (pll->vel > pll->vel_limit || pll->vel < -pll->vel_limit)
    {
        pll->integ = 0.0f;
        pll->vel = 0.0f;
        pll->theta = theta;
    }
}