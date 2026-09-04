// ============================================================
// as5047.c — AS5047 编码器驱动（usr/drv，v2 直连版，同步读角）
//
// 芯片协议：SPI Mode1(CPOL=0,CPHA=1)/16bit/14bit，分辨率 16384
// 读角序列：段1 发 0x7FFF（弃响应）→ 段2 发 0x0000 收角度帧
// 角度帧：bit14 错误标志；bit13..0 角度
// 底层经 enc_spi_engine 直连本板编码器 SPI（platform.h）。
// ============================================================

#include "encoder_drivers.h"

#define AS5047_RESOLUTION 16384U
#define AS5047_CMD_READ 0x7FFFU
#define AS5047_CMD_NOP 0x0000U
#define AS5047_ERR_FLAG 0x4000U
#define AS5047_ANGLE_MASK 0x3FFFU

typedef struct
{
    eDeviceStatus dstate;
    eEncoderType type;
    uint16_t cmd_read; // 段1 tx（16bit 值的内存视图）
    uint16_t cmd_nop;  // 段2 tx
    uint8_t rx1[2];    // 段1 rx（弃用）
    uint8_t rx2[2];    // 段2 rx（角度帧）
    uint16_t raw;
    uint32_t ts;
} tAS5047_ctx;

static bool AS5047_init(EncoderChipHandle h, eEncoderType type)
{
    tAS5047_ctx *ctx = (tAS5047_ctx *)h;
    if (!ctx)
        return false;

    if (!enc_spi_set_mode(0U, 1U, 16U)) // 芯片协议：Mode1/16bit
        return false;

    ctx->type = type; // 编码器类型
    ctx->cmd_read = AS5047_CMD_READ;
    ctx->cmd_nop = AS5047_CMD_NOP;
    ctx->raw = 0U;
    ctx->ts = 0U;
    ctx->dstate = DEV_ONLINE;
    return true;
}

static bool AS5047_read_angle(EncoderChipHandle h, uint16_t *raw, uint32_t *ts_ms)
{
    tAS5047_ctx *ctx = (tAS5047_ctx *)h;
    if (!ctx || !raw || !ts_ms)
        return false;

    tEncXferSeg segs[2];
    segs[0].tx = (const uint8_t *)&ctx->cmd_read;
    segs[0].rx = ctx->rx1;
    segs[0].len = 2U;
    segs[1].tx = (const uint8_t *)&ctx->cmd_nop;
    segs[1].rx = ctx->rx2;
    segs[1].len = 2U;

    if (!enc_engine_read(segs, ctx->type, 2U))
    {
        ctx->dstate = DEV_RUN_ERROR;
        return false;
    }

    uint16_t frame = (uint16_t)(ctx->rx2[0] | ((uint16_t)ctx->rx2[1] << 8));
    if (frame & AS5047_ERR_FLAG)
    {
        ctx->dstate = DEV_RUN_ERROR;
        return false;
    }

    ctx->raw = frame & AS5047_ANGLE_MASK;
    ctx->ts = enc_tick_ms();
    ctx->dstate = DEV_RUNNING;
    *raw = ctx->raw;
    *ts_ms = ctx->ts;
    return true;
}

static bool AS5047_get_resolution(EncoderChipHandle h, uint16_t *res)
{
    tAS5047_ctx *ctx = (tAS5047_ctx *)h;
    if (!ctx || !res)
        return false;
    *res = AS5047_RESOLUTION;
    return true;
}

static void AS5047_reset(EncoderChipHandle h)
{
    tAS5047_ctx *ctx = (tAS5047_ctx *)h;
    if (!ctx)
        return;
    enc_engine_abort(ctx->type);
    ctx->raw = 0U;
    ctx->ts = 0U;
    ctx->dstate = DEV_ONLINE;
}

static uint8_t AS5047_get_state(EncoderChipHandle h)
{
    tAS5047_ctx *ctx = (tAS5047_ctx *)h;
    return (uint8_t)(ctx ? ctx->dstate : DEV_OFFLINE);
}

const tEncoderDriverOps AS5047_driver_ops = {
    .init = AS5047_init,
    .read_angle = AS5047_read_angle,
    .get_resolution = AS5047_get_resolution,
    .reset = AS5047_reset,
    .get_state = AS5047_get_state,
};

EncoderChipHandle AS5047_create(void)
{
    tAS5047_ctx *ctx = (tAS5047_ctx *)calloc(1U, sizeof(tAS5047_ctx));
    if (!ctx)
        return NULL;
    ctx->dstate = DEV_OFFLINE;
    return (EncoderChipHandle)ctx;
}

void AS5047_destroy(EncoderChipHandle h)
{
    free(h);
}
