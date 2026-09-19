// ============================================================
// enc_as5047.c — AS5047 编码器驱动（板级，SPI 同步读角）
//
// 芯片协议：SPI Mode1(CPOL=0,CPHA=1)/16bit/14bit，分辨率 16384
// 读角序列：段1 发 0x7FFF（弃响应）→ 段2 发 0x0000 收角度帧
// 角度帧：bit14 错误标志；bit13..0 角度
// 实例形态：按内/外编码器固定的静态实例（无堆分配、无 create/destroy），
//           每实例含 ops / 引擎 / 配置 / 运行时缓冲。
// ============================================================
#include "encoder_drivers.h"

// ---------- 本板配置 ----------
#define AS5047_RESOLUTION 16384U
#define AS5047_CMD_READ 0x7FFFU
#define AS5047_CMD_NOP 0x0000U
#define AS5047_ERR_FLAG 0x4000U
#define AS5047_ANGLE_MASK 0x3FFFU

// ---------- 实例 handle ----------
typedef struct
{
    EncEngineHandle engine; // SPI 引擎（内/外共用）
    eEncoderType type;      // 配置：内/外编码器
    uint16_t cmd_read;      // 配置：段1 tx
    uint16_t cmd_nop;       // 配置：段2 tx
    uint16_t resolution;    // 配置：单圈分辨率
    uint8_t rx1[2];         // 运行时：段1 rx（弃用）
    uint8_t rx2[2];         // 运行时：段2 rx（角度帧）
    uint16_t raw;
} tAS5047;

static bool AS5047_init(EncoderChipHandle h, eEncoderType type);
static bool AS5047_read_angle(EncoderChipHandle h, uint16_t *raw, uint32_t *ts_ms);
static bool AS5047_get_resolution(EncoderChipHandle h, uint16_t *res);
static void AS5047_reset(EncoderChipHandle h);

const tEncoderDriverOps AS5047_driver_ops = {
    .init = AS5047_init,
    .read_angle = AS5047_read_angle,
    .get_resolution = AS5047_get_resolution,
    .reset = AS5047_reset,
};

// 注册实例
EncoderChipHandle AS5047_register_handle(void)
{
    tAS5047 *h = (tAS5047 *)enc_chip_get_handle();
    if (!h)
        return NULL;

    return (EncoderChipHandle)h;
}
// 注销实例
void AS5047_unregister_handle(EncoderChipHandle h)
{
    tAS5047 *ctx = (tAS5047 *)h;
    if (!ctx)
        return;
    free(ctx);
    ctx = NULL;
}

// ---- ops 实现 ----

static bool AS5047_init(EncoderChipHandle h, eEncoderType type)
{
    tAS5047 *ctx = (tAS5047 *)h;
    if (!ctx)
        return false;
    ctx->type = type;
    ctx->engine = enc_engine_get_handle();
    if (!enc_spi_set_mode(ctx->engine, 0U, 1U, 16U)) // 芯片协议：Mode1/16bit
        return false;

    ctx->raw = 0U;
    return true;
}

static bool AS5047_read_angle(EncoderChipHandle h, uint16_t *raw, uint32_t *ts_ms)
{
    tAS5047 *ctx = (tAS5047 *)h;
    if (!ctx || !raw || !ts_ms)
        return false;

    tEncXferSeg segs[2];
    segs[0].tx = (const uint8_t *)&ctx->cmd_read;
    segs[0].rx = ctx->rx1;
    segs[0].len = 2U;
    segs[1].tx = (const uint8_t *)&ctx->cmd_nop;
    segs[1].rx = ctx->rx2;
    segs[1].len = 2U;

    if (!enc_engine_read(ctx->engine, segs, ctx->type, 2U, ts_ms))
        return false;

    uint16_t frame = (uint16_t)(ctx->rx2[0] | ((uint16_t)ctx->rx2[1] << 8));
    if (frame & AS5047_ERR_FLAG)
        return false;

    ctx->raw = (uint16_t)(frame & AS5047_ANGLE_MASK);
    *raw = ctx->raw;
    return true;
}

static bool AS5047_get_resolution(EncoderChipHandle h, uint16_t *res)
{
    tAS5047 *ctx = (tAS5047 *)h;
    if (!ctx || !res)
        return false;
    *res = ctx->resolution;
    return true;
}

static void AS5047_reset(EncoderChipHandle h)
{
    tAS5047 *ctx = (tAS5047 *)h;
    if (!ctx)
        return;
    enc_engine_abort(ctx->engine, ctx->type);
    ctx->raw = 0U;
}
