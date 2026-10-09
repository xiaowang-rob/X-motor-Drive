#include "obs.h"
#include "bsp_math.h"
#include "bsp_cfg.h"

#include "parameters.h"

// 位置累积
static float pos_accumulate(tPosAcc *acc, float theta)
{
    float angle_delta = theta - acc->last_angle;
    if (angle_delta < -MATH_PI)
        acc->num_turns++;
    else if (angle_delta > MATH_PI)
        acc->num_turns--;
    acc->last_angle = theta;
    return (theta - acc->zero_angle) + acc->num_turns * MATH_2PI;
}

#define ENCODER_PLL_KP 80.0f
#define ENCODER_PLL_KI 2000.0f
#define ENCODER_PLL_INTEG_LIMIT 0.1745f // 积分限值  ±10°
#define ENCODER_VEL_PHYS_LIMIT 1046.0f  // rad/s 物理上限（≈10k rpm）

#define HFISMO_PLL_KP 80.0f
#define HFISMO_PLL_KI 2000.0f
#define HFISMO_PLL_INTEG_LIMIT 0.1745f // 积分限值  ±10°
#define HFISMO_VEL_PHYS_LIMIT 2000.0f  // rad/s 物理上限（≈10k rpm）

// 观测器初始化
bool obs_init(tObs *obs)
{
    obs->type = (eObsList)g_param.obs_type;

    memset(&obs->pacc, 0, sizeof(tPosAcc));

    switch (obs->type)
    {
    case NONE_OBS:

        break;
    case ENCODER_SPI:
        // 配置编码器设备驱动
        if (!bsp_enc_init((eEncoderMode)g_param.ienc_mode,
                          (eEncoderMode)g_param.eenc_mode, (eEncoderChip)g_param.eenc_chip))
            return false;
    case ENCODER_ABZ:
    case ENCODER_SINCOS:

        pll_init(&obs->pll, ENCODER_PLL_KP, ENCODER_PLL_KI,
                 ENCODER_PLL_INTEG_LIMIT, ENCODER_VEL_PHYS_LIMIT);
        break;
    case HFI_SMO:

        pll_init(&obs->pll, HFISMO_PLL_KP, HFISMO_PLL_KI,
                 HFISMO_PLL_INTEG_LIMIT, HFISMO_VEL_PHYS_LIMIT);
        break;
    default:
        break;
    }
    return true;
}

bool obs_loop_task(tObs *obs)
{
    switch (obs->type)
    {
    case NONE_OBS:
        break;
    case ENCODER_SPI:
    case ENCODER_SINCOS:
        // TODO:这里暂时只是外部编码器 后续可以改
        encoder_task(&g_enc_ext);
        obs->theta_m = encoder_get_angle_abs(&g_enc_ext);
        return g_enc_ext.dstate == DEV_RUNNING;

    case ENCODER_ABZ:
        break;

    case HFI_SMO:
        break;
    default:
        break;
    }
    return true;
}

void obs_tim_task(tObs *obs, float ts,
                  float *theta_e, float *vel, float *pos)
{

    switch (obs->type)
    {
    case NONE_OBS:

        break;
    case ENCODER_SPI:
    case ENCODER_SINCOS:
    {
        // TODO:20khz pll跟随1khz角度变化 需要1khz的角度突变处理
        *pos = pos_accumulate(&obs->pacc, obs->theta_m);
        pll_update(&obs->pll, *pos, ts);

        float theta_mech = obs->pll.theta;
        *theta_e = (theta_mech - g_param.theta_offset) * g_param.motor_polepairs * (g_param.positive_dir ? 1 : -1);
        *theta_e = normalize_angle_2pi(*theta_e);
        *vel = obs->pll.vel;
        break;
    }
    case ENCODER_ABZ:
        // TODO:ABZ编码器适配
        break;

    case HFI_SMO:
        // TODO:HFI+SMO
        break;
    default:
        break;
    }
}

static void obs_set_zero(tObs *obs)
{

    switch (obs->type)
    {
    case NONE_OBS:

        break;
    case ENCODER_SPI:
    case ENCODER_SINCOS:
    {
        obs->pacc.zero_angle = obs->theta_m;
        obs->pacc.num_turns = 0;
        obs->pacc.last_angle = obs->theta_m;
        break;
    }
    case ENCODER_ABZ:
        // TODO:ABZ编码器适配
        break;

    case HFI_SMO:
        // TODO:HFI+SMO
        break;
    default:
        break;
    }
}

void obs_set_max_min(tObs *obs, float max_pos, float min_pos)
{
    obs->pacc.max_pos = max_pos;
    obs->pacc.min_pos = min_pos;
}