// ============================================================
// mt6816.c — MT6816 编码器驱动（usr/drv，v2 直连版，同步读角）
//
// 芯片协议：SPI Mode3(CPOL=1,CPHA=1)/16bit/14bit，分辨率 16384
// 读角序列：段1 发 0x83FF → 段2 发 0x84FF 收有效数据帧
// 解析：奇偶校验 + 磁场告警位（见下）
// ============================================================

#include "encoder_drivers.h"

#define MT6816_RESOLUTION 16384U
#define MT6816_CMD_HIGH 0x83FFU
#define MT6816_CMD_LOW 0x84FFU
#define MT6816_MAG_WARN (1U << 1)
#define MT6816_PARITY_BIT (1U << 0)

typedef struct
{
    eEncoderType type;
    uint16_t cmd_high;
    uint16_t cmd_low;
    uint8_t rx1[2];
    uint8_t rx2[2];
    uint16_t raw;
} tMT6816_ctx;

static bool parity_odd(uint16_t v)
{
    v ^= (uint16_t)(v >> 8);
    v ^= (uint16_t)(v >> 4);
    v ^= (uint16_t)(v >> 2);
    v ^= (uint16_t)(v >> 1);
    return (v & 1U) != 0U;
}

static bool MT6816_init(EncoderChipHandle h, eEncoderType type)
{
    tMT6816_ctx *ctx = (tMT6816_ctx *)h;
    if (!ctx)
        return false;

    if (!enc_spi_set_mode(1U, 1U, 16U)) // 芯片协议：Mode3/16bit
        return false;

    ctx->type = type;
    ctx->cmd_high = MT6816_CMD_HIGH;
    ctx->cmd_low = MT6816_CMD_LOW;
    ctx->raw = 0U;
    return true;
}

static bool MT6816_read_angle(EncoderChipHandle h, uint16_t *raw, uint32_t *ts_ms)
{
    tMT6816_ctx *ctx = (tMT6816_ctx *)h;
    if (!ctx || !raw || !ts_ms)
        return false;

    tEncXferSeg segs[2];
    segs[0].tx = (const uint8_t *)&ctx->cmd_high;
    segs[0].rx = ctx->rx1;
    segs[0].len = 2U;
    segs[1].tx = (const uint8_t *)&ctx->cmd_low;
    segs[1].rx = ctx->rx2;
    segs[1].len = 2U;

    if (!enc_engine_read(segs, ctx->type, 2U, ts_ms))
        return false;

    uint16_t high = (uint16_t)(ctx->rx1[0] | ((uint16_t)ctx->rx1[1] << 8));
    uint16_t low = (uint16_t)(ctx->rx2[0] | ((uint16_t)ctx->rx2[1] << 8));

    if (low & MT6816_MAG_WARN) // 磁场告警 → 数据不可信
        return false;

    uint16_t bits = (uint16_t)(((high & 0x00FFU) << 7) | ((low & 0x00FEU) >> 1));
    bool parity = (low & MT6816_PARITY_BIT) != 0U;
    if (parity_odd(bits) != parity)
        return false;

    ctx->raw = bits >> 1;
    *raw = ctx->raw;
    return true;
}

static bool MT6816_get_resolution(EncoderChipHandle h, uint16_t *res)
{
    tMT6816_ctx *ctx = (tMT6816_ctx *)h;
    if (!ctx || !res)
        return false;
    *res = MT6816_RESOLUTION;
    return true;
}

static void MT6816_reset(EncoderChipHandle h)
{
    tMT6816_ctx *ctx = (tMT6816_ctx *)h;
    if (!ctx)
        return;
    enc_engine_abort(ctx->type);
    ctx->raw = 0U;
}

const tEncoderDriverOps MT6816_driver_ops = {
    .init = MT6816_init,
    .read_angle = MT6816_read_angle,
    .get_resolution = MT6816_get_resolution,
    .reset = MT6816_reset,
};

EncoderChipHandle MT6816_create(void)
{
    tMT6816_ctx *ctx = (tMT6816_ctx *)calloc(1U, sizeof(tMT6816_ctx));
    if (!ctx)
        return NULL;
    return (EncoderChipHandle)ctx;
}

void MT6816_destroy(EncoderChipHandle h)
{
    free(h);
    h = NULL;
}
