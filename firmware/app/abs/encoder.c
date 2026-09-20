// ============================================================
// encoder.c — 编码器业务对象（abs，纯逻辑）
//
// 硬件动作经 ops + handle 直达驱动：enc->ops->read(enc->handle, ...)。
// 型号、外设与 CS 引脚等板级事实不在本层出现。
// ============================================================

#include "encoder.h"

#include "math_fast.h"

bool encoder_init(tEncoder *enc, eEncoderMode mode)
{
    if (!enc || !enc->ops || !enc->handle)
        return false;
    if (!enc->ops->open || !enc->ops->read)
        return false;

    // 只重置业务字段；ops / handle 是装配结果，不在本层改动
    enc->mode = mode;
    enc->dstate = DEV_OFFLINE;
    enc->resolution = 0U;
    enc->rad_per_lsb = 0.0f;
    enc->angle_abs = 0.0f;
    enc->valid_counter = 0U;

    uint16_t resolution = 0U;
    if (!enc->ops->open(enc->handle, &resolution) || resolution == 0U)
    {
        enc->dstate = DEV_RUN_ERROR;
        return false;
    }

    enc->resolution = resolution;
    enc->rad_per_lsb = MATH_2PI / (float)resolution;
    enc->dstate = DEV_ONLINE;
    return true;
}

void encoder_task(tEncoder *enc)
{
    if (!enc || !enc->ops || !enc->ops->read)
        return;

    uint16_t raw = 0U;
    uint32_t ts_ms = 0U;
    if (!enc->ops->read(enc->handle, &raw, &ts_ms))
    {
        // 读取失败：滑动计数向失效方向走
        enc->valid_counter = (enc->valid_counter < ENCODER_VALID_COUNT_MAX)
                                 ? (uint16_t)(enc->valid_counter + 10U)
                                 : ENCODER_VALID_COUNT_MAX;
        if (enc->valid_counter > ENCODER_ERR_VALID_LIMIT)
            enc->dstate = DEV_RUN_ERROR;
        return;
    }

    // 读取成功：计数向有效方向衰减
    enc->valid_counter = (enc->valid_counter > 0U) ? (uint16_t)(enc->valid_counter - 1U) : 0U;
    if (enc->valid_counter <= ENCODER_ERR_VALID_LIMIT)
        enc->dstate = DEV_RUNNING;

    enc->angle_abs = (float)raw * enc->rad_per_lsb;
}
