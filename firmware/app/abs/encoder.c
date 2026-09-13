// ============================================================
// encoder.c — 编码器业务对象
//
// 输入：tEncoderDriverOps（同步 read_angle）+ 芯片句柄
// 输出：多圈位置 pos、绝对角 angle_abs、M-T 速度 vel、PLL 平滑角度/速度、
//       数据有效性 valid_counter（连续读数失败的滑动指示）
// ============================================================

#include "encoder.h"

#include "math_fast.h"

bool encoder_init(tEncoder *enc, const tEncoderDriverOps *ops,
                  EncoderChipHandle handle, eEncoderType type)
{
    if (!enc || !ops || !handle)
        return false;

    memset(enc, 0, sizeof(tEncoder));
    enc->drv_ops = ops;
    enc->drv_handle = handle;
    enc->type = type;
    enc->dstate = DEV_OFFLINE;

    if (!ops->init(handle, type))
        return false;

    if (!ops->get_resolution(handle, &enc->resolution) || enc->resolution == 0U)
        return false;
    enc->rad_per_lsb = MATH_2PI / (float)enc->resolution;

    enc->first_run = true;
    enc->valid_counter = 0U;
    enc->dstate = DEV_ONLINE;
    return true;
}

void encoder_task(tEncoder *enc)
{
    if (!enc || !enc->drv_ops)
        return;

    uint16_t raw;
    uint32_t ts;
    if (!enc->drv_ops->read_angle(enc->drv_handle, &raw, &ts))
    {
        // 读取失败：滑动计数向失效方向走
        enc->valid_counter = (enc->valid_counter < 110U) ? (uint16_t)(enc->valid_counter + 10U) : 110U;
        if (enc->valid_counter > ENCODER_ERR_VALID_LIMIT)
            enc->dstate = DEV_RUN_ERROR;
        return;
    }

    // 读取成功：计数向有效方向衰减
    enc->valid_counter = (enc->valid_counter > 0U) ? (uint16_t)(enc->valid_counter - 1U) : 0U;
    if (enc->valid_counter <= ENCODER_ERR_VALID_LIMIT)
        enc->dstate = DEV_RUNNING;

    float angle_abs = (float)raw * enc->rad_per_lsb;

    if (enc->first_run)
    {
        enc->last_raw_angle = raw;
        enc->pos_offset = (float)raw;
        enc->num_turns = 0;
        enc->pos = 0.0f;
        enc->last_angle_abs = angle_abs;
        enc->last_ts_ms = ts;
        enc->pll_theta = angle_abs;
        enc->first_run = false;
        enc->angle_abs = angle_abs;
        return;
    }

    const int16_t half_res = (int16_t)(enc->resolution / 2U);

    // 多圈累计：跨越半圈判绕零方向
    int16_t delta_raw = (int16_t)(raw - enc->last_raw_angle);
    if (delta_raw < -half_res)
        enc->num_turns++;
    else if (delta_raw > half_res)
        enc->num_turns--;

    // 连续位置（零位偏移 + 圈数）
    enc->pos = ((float)raw - enc->pos_offset) * enc->rad_per_lsb + (float)enc->num_turns * MATH_2PI;

    // M/T 测速（未平滑，可作为快速量；平滑速度用 PLL）
    float dt = (float)(ts - enc->last_ts_ms) / 1000.0f;
    if (dt > 0.001f)
    {
        float delta_angle = normalize_angle_pi(angle_abs - enc->last_angle_abs);
        enc->vel = delta_angle / dt;
        if (FABSF(enc->vel) > ENCODER_VEL_PHYS_LIMIT)
            enc->vel = 0.0f;
        enc->last_ts_ms = ts;
        enc->last_angle_abs = angle_abs;
    }

    enc->last_raw_angle = raw;
    enc->angle_abs = angle_abs;
}

// PLL 跟踪：把单圈绝对角平滑成 theta，并给出平滑速度（比 M-T 噪声小）
void encoder_pll_update(tEncoder *enc, float dt)
{
    if (!enc || dt <= 0.0f)
        return;

    enc->pll_theta_delta = normalize_angle_pi(enc->angle_abs - enc->pll_theta);

    enc->pll_integ += enc->pll_theta_delta * dt;
    if (enc->pll_integ > ENCODER_PLL_INTEG_LIMIT)
        enc->pll_integ = ENCODER_PLL_INTEG_LIMIT;
    if (enc->pll_integ < -ENCODER_PLL_INTEG_LIMIT)
        enc->pll_integ = -ENCODER_PLL_INTEG_LIMIT;

    float estimated_speed = ENCODER_PLL_KP * enc->pll_theta_delta + ENCODER_PLL_KI * enc->pll_integ;
    enc->pll_theta += estimated_speed * dt;
    enc->pll_theta = normalize_angle_2pi(enc->pll_theta);
    enc->pll_vel = (FABSF(estimated_speed) < 0.05f) ? 0.0f : estimated_speed;

    // 超物理速度视为失锁，重锁到当前角度
    if (enc->pll_vel > ENCODER_VEL_PHYS_LIMIT || enc->pll_vel < -ENCODER_VEL_PHYS_LIMIT)
    {
        enc->pll_integ = 0.0f;
        enc->pll_vel = 0.0f;
        enc->pll_theta = enc->angle_abs;
    }
}

void encoder_set_zero(tEncoder *enc)
{
    if (!enc)
        return;
    enc->pos_offset = (float)enc->last_raw_angle;
    enc->num_turns = 0;
    enc->pos = 0.0f;
    enc->pll_theta = enc->angle_abs;
    enc->pll_integ = 0.0f;
}
