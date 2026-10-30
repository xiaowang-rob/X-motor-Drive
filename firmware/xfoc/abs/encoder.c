// ============================================================
// encoder.c — 编码器业务对象（abs，纯逻辑）
//
// 硬件动作经 ops + handle 直达驱动：enc->ops->read(enc->handle->dev, ...)。
// 型号、外设与 CS 引脚等板级事实不在本层出现。
// ============================================================

#include "encoder.h"

bool encoder_init(tEncoder *enc, const tEncDevHandle *handle) {
    if (!enc || !handle)
        return false;

    enc->handle = handle;

    // 只重置业务字段；ops / handle 是装配结果，不在本层改动
    enc->dstate = DEV_OFFLINE;
    memset(&enc->sample, 0U, sizeof(enc->sample));
    enc->valid_counter = 0U;

    if (!enc->handle->ops->open(enc->handle->dev)) {
        enc->dstate = DEV_RUN_ERROR;
        return false;
    }

    enc->dstate = DEV_ONLINE;
    return true;
}

void encoder_task(tEncoder *enc) {
    if (!enc)
        return;

    if (!enc->handle->ops->read(enc->handle->dev, &enc->sample)) {
        // 读取失败：滑动计数向失效方向走
        enc->valid_counter = (enc->valid_counter < ENCODER_VALID_COUNT_MAX)
                                 ? (uint16_t)(enc->valid_counter + 10U)
                                 : ENCODER_VALID_COUNT_MAX;
        if (enc->valid_counter > ENCODER_ERR_VALID_LIMIT)
            enc->dstate = DEV_RUN_ERROR;
        return;
    }

    // 读取成功：计数向有效方向衰减
    enc->valid_counter =
        (enc->valid_counter > 0U) ? (uint16_t)(enc->valid_counter - 1U) : 0U;
    if (enc->valid_counter <= ENCODER_ERR_VALID_LIMIT)
        enc->dstate = DEV_RUNNING;
}
