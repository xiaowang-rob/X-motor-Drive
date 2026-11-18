#include "enc_manager.h"

#include "xmath.h"

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
// 外接编码器初始化
bool em_init(tParameter *param) {
    const tEncDevHandle *handle;
    void *cfg = NULL;
    switch (param->rotor_enc) {
    case ENC_NONE:
        return true;
    case ENC_INT:
        handle = Get_EncIntHandle();
        break;
    case ENC_HALL:
        handle = Get_EncHallHandle();
        tHallEncCfg hall_cfg;
        hall_cfg.sequence[0] = param->hall_table_0;
        hall_cfg.sequence[1] = param->hall_table_1;
        hall_cfg.sequence[2] = param->hall_table_2;
        hall_cfg.sequence[3] = param->hall_table_3;
        hall_cfg.sequence[4] = param->hall_table_4;
        hall_cfg.sequence[5] = param->hall_table_5;
        hall_cfg.prescaler = param->hall_psc;
        hall_cfg.filter = param->hall_filter;

        cfg = &hall_cfg;
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
        tAbiCfg abi_cfg;
        abi_cfg.enc_counts_per_rev = param->abi_count;
        abi_cfg.filter = param->abi_filter;
        abi_cfg.index_falling = param->abi_index;
        cfg = &abi_cfg;
        break;
    case ENC_SINCOS:
        handle = Get_EncSinCosHandle();
        tSinCosEncCfg sincos_cfg;
        sincos_cfg.sin_offset = param->sin_offset;
        sincos_cfg.cos_offset = param->cos_offset;
        sincos_cfg.sin_gain = param->sin_amp;
        sincos_cfg.cos_gain = param->cos_amp;
        sincos_cfg.filter_alpha = param->sincos_fl;
        cfg = &sincos_cfg;
        break;

    default:
        return false;
    }

    pll_init(&g_encm.pll,
             ENCODER_PLL_KP,
             ENCODER_PLL_KI,
             ENCODER_PLL_INTEG_LIMIT,
             ENCODER_VEL_PHYS_LIMIT);

    g_encm.enc_active = enc_init(&g_enc, handle, cfg);
    return g_encm.enc_active;
}

void em_loop_task(void) {
    if (!g_encm.enc_active)
        return;
    // TODO:这里处理不同编码器角度
    enc_task(&g_enc);
    g_encm.theta_r = g_enc.sample.angle_rad;
}

void em_set_pos_zero(void) {
    g_encm.pacc.zero_angle = g_encm.theta_r;
    g_encm.pacc.num_turns = 0;
    g_encm.pacc.last_angle = g_encm.theta_r;
}

void em_set_pos_limit(void) {
    if (g_encm.pos > 0)
        g_encm.pacc.max_pos = g_encm.pos;
    else
        g_encm.pacc.min_pos = g_encm.pos;
}

void em_theta_process_task(float ts, float *th_e, float *th_r, float *vel,
                           float *pos) {
    if (!g_encm.enc_active)
        return;
    pll_update(&g_encm.pll, g_encm.theta_r, ts);
    if (g_param.theta_source == THETA_PLL)
        *th_r = g_encm.pll.theta;
    else
        *th_r = g_encm.theta_r;

    g_encm.pos = pos_accumulate(&g_encm.pacc, g_encm.theta_r);

    g_encm.theta_e = (*th_r - g_param.theta_offset) * g_param.motor_polepairs *
                     (g_param.positive_dir ? 1 : -1);
    g_encm.theta_e = XANGLE_2PI(g_encm.theta_e);
    g_encm.vel = g_encm.pll.vel;

    *th_e = g_encm.theta_e;
    *vel = g_encm.vel;
    *pos = g_encm.pos;
}
