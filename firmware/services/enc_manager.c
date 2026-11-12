#include "enc_manager.h"
#include "xfoc_main.h"

#include "enc_abi.h"
#include "enc_as5047.h"
#include "enc_hall.h"
#include "enc_mt6816.h"
#include "enc_mt6835.h"
#include "enc_sincos.h"

#define ENCODER_PLL_WN 300.0f  // 自然频率 决定跟踪带宽
#define ENCODER_PLL_ZETA 0.707 // 阻尼比 越大超调越小响应越慢
#define ENCODER_PLL_KP (2.0f * ENCODER_PLL_ZETA * ENCODER_PLL_WN)
#define ENCODER_PLL_KI (ENCODER_PLL_WN * ENCODER_PLL_WN)
#define ENCODER_PLL_INTEG_LIMIT 0.1745f // 积分限值  ±10°
#define ENCODER_VEL_PHYS_LIMIT 1046.0f  // rad/s 物理上限（≈10k rpm）

tEncoder g_enc;
tEncManager g_encm;

// 外接编码器初始化
bool em_init(tParameter *param) {
    const tEncDevHandle *handle;

    switch (param->rotor_enc) {
    case ENC_NONE:
        return true;
    case ENC_INT:
        handle = Get_EncIntHandle();
        break;
    case ENC_HALL:
        handle = Get_EncHallHandle();
        break;
    case ENC_SPI:
        switch (param->enc_chip) {
        case ENC_MT6816:
            handle = Get_EncMT6816Handle();
            break;
        case ENC_MT6835:
            handle = Get_EncMT6835Handle();
            break;
        case ENC_AS5047:
            handle = Get_EncAS5047Handle();
            break;
        default:
            return false;
        }
        break;
    case ENC_ABI:
        handle = Get_EncAbiHandle();
        break;
    case ENC_SINCOS:
        handle = Get_EncSinCosHandle();
        break;

    default:
        return false;
    }
    pll_init(&g_encm.pll,
             ENCODER_PLL_KP,
             ENCODER_PLL_KI,
             ENCODER_PLL_INTEG_LIMIT,
             ENCODER_VEL_PHYS_LIMIT);

    g_encm.enc_active = enc_init(&g_enc, handle);
    return g_encm.enc_active;
}

void em_loop_task(void) {
    if (!g_encm.enc_active)
        return;
    // TODO:这里处理不同编码器角度
    enc_task(&g_enc);
    g_encm.theta_r = g_enc.sample.angle_rad;
}

void em_tim_task(float ts,float *th_e,float *th_r,float *vel) {
    if (!g_encm.enc_active)
        return;
    pll_update(&g_encm.pll, g_encm.theta_r, ts);
    if (g_param.theta_source == THETA_PLL)
        *th_r = g_encm.pll.theta;
    else
        *th_r = g_encm.theta_r;

    *th_e = (*th_r - g_param.theta_offset) *
                           g_param.motor_polepairs *
                           (g_param.positive_dir ? 1 : -1);

    *th_e = XANGLE_2PI(*th_e);
    *vel = g_encm.pll.vel;
}
