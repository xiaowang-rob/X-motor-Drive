// ============================================================
// encoder.c — 编码器业务对象（abs，纯逻辑）
//
// 输入：板级钩子 encoder_board_read（同步读原始角 + 时间戳）
// 输出：angle_abs、数据有效性 valid_counter、设备状态 dstate
//
// 无 ops 表、无 void* 句柄：直接调用板级钩子，链接期解析。
// ============================================================

#include "encoder.h"

#include "encoder_board.h"
#include "math_fast.h"

bool encoder_init(tEncoder *enc, eEncoderType type, eEncoderChipId chip)
{
    if (!enc)
        return false;

    memset(enc, 0, sizeof(tEncoder));
    enc->type = type;
    enc->dstate = DEV_OFFLINE;

    uint16_t resolution = 0U;
    if (!encoder_board_open(type, chip, &resolution) || resolution == 0U)
        return false;

    enc->resolution = resolution;
    enc->rad_per_lsb = MATH_2PI / (float)resolution;
    enc->valid_counter = 0U;
    enc->dstate = DEV_ONLINE;
    return true;
}

void encoder_task(tEncoder *enc)
{
    if (!enc)
        return;

    uint16_t raw = 0U;
    uint32_t ts_ms = 0U;
    if (!encoder_board_read(enc->type, &raw, &ts_ms))
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
