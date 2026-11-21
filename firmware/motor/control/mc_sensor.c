#include "mc_sensor.h"

#include "xmath.h"
// 位置累积
static float pos_accumulate(tPosAcc *acc, float theta) {
    float angle_delta = theta - acc->last_angle;
    if (angle_delta < -XMATH_PI)
        acc->num_turns++;
    else if (angle_delta > XMATH_PI)
        acc->num_turns--;
    acc->last_angle = theta;
    return (theta - acc->zero_angle) + acc->num_turns * XMATH_2PI;
}

#define ENCODER_PLL_WN 300.0f  // 自然频率 决定跟踪带宽
#define ENCODER_PLL_ZETA 0.707 // 阻尼比 越大超调越小响应越慢
#define ENCODER_PLL_KP (2.0f * ENCODER_PLL_ZETA * ENCODER_PLL_WN)
#define ENCODER_PLL_KI (ENCODER_PLL_WN * ENCODER_PLL_WN)
#define ENCODER_PLL_INTEG_LIMIT 0.1745f // 积分限值  ±10°
#define ENCODER_VEL_PHYS_LIMIT 1046.0f  // rad/s 物理上限（≈10k rpm）

void mc_sensor_init(tMcSensor *sen) {
    pll_init(&sen->pll,
             ENCODER_PLL_KP,
             ENCODER_PLL_KI,
             ENCODER_PLL_INTEG_LIMIT,
             ENCODER_VEL_PHYS_LIMIT);
    // TODO:初始化无感算法
}

void mc_sensor_theta_process(tMcSensor *sen, float ts, uint8_t polepairs) {
    // TODO: 区分输入的是编码器 还是 观测器角度 编码器角度是电角度还是机械角度
    pll_update(&sen->pll, sen->theta_r_raw, ts);

    sen->theta_r = sen->pll.theta;
    sen->pos = pos_accumulate(&sen->pacc, sen->theta_r);
    sen->theta_e = sen->theta_r * polepairs;
    sen->theta_e = XANGLE_2PI(sen->theta_e);
    sen->vel = sen->pll.vel;
}

void mc_sensor_set_pos_zero(tMcSensor *sen) {
    sen->pacc.zero_angle = sen->theta_r;
    sen->pacc.num_turns = 0;
    sen->pacc.last_angle = sen->theta_r;
}

void mc_sensor_set_pos_limit(tMcSensor *sen) {
    if (sen->pos > 0)
        sen->pacc.max_pos = sen->pos;
    else
        sen->pacc.min_pos = sen->pos;
}