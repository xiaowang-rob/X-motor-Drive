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

    enc->angle_abs = (float)raw * enc->rad_per_lsb;
}
