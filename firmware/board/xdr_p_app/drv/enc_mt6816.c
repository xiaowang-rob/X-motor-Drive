// ============================================================
// enc_mt6816.c — MT6816 编码器驱动（板级，SPI 同步读角）
//
// 芯片协议：SPI Mode3(CPOL=1,CPHA=1)/16bit/14bit，分辨率 16384
// 读角序列：段1 发 0x83FF → 段2 发 0x84FF 收有效数据帧
// 解析：奇偶校验 + 磁场告警位
// 实例形态：按内/外编码器固定的静态实例（无堆分配、无 create/destroy），
//           每实例含 ops / 引擎 / 配置 / 运行时缓冲。
// ============================================================
#include "encoder_drivers.h"

// ---------- 本板配置 ----------
#define MT6816_RESOLUTION 16384U
#define MT6816_CMD_HIGH 0x83FFU
#define MT6816_CMD_LOW 0x84FFU
#define MT6816_MAG_WARN (1U << 1)
#define MT6816_PARITY_BIT (1U << 0)

// ---------- 实例 handle ----------
typedef struct
{
    const tEncoderDriverOps *ops; // 该实例的操作表
    EncEngineHandle engine;       // SPI 引擎（内/外共用）
    eEncoderType type;            // 配置：内/外编码器
    uint16_t cmd_high;            // 配置：段1 tx
    uint16_t cmd_low;             // 配置：段2 tx
    uint16_t resolution;          // 配置：单圈分辨率
    uint8_t rx1[2];               // 运行时：段1 rx
    uint8_t rx2[2];               // 运行时：段2 rx
    uint16_t raw;
} tMT6816;

static bool parity_odd(uint16_t v)
{
    v ^= (uint16_t)(v >> 8);
    v ^= (uint16_t)(v >> 4);
    v ^= (uint16_t)(v >> 2);
    v ^= (uint16_t)(v >> 1);
    return (v & 1U) != 0U;
}

static bool MT6816_init(EncoderChipHandle h, eEncoderType type);
static bool MT6816_read_angle(EncoderChipHandle h, uint16_t *raw, uint32_t *ts_ms);
static bool MT6816_get_resolution(EncoderChipHandle h, uint16_t *res);
static void MT6816_reset(EncoderChipHandle h);

const tEncoderDriverOps MT6816_driver_ops = {
    .init = MT6816_init,
    .read_angle = MT6816_read_angle,
    .get_resolution = MT6816_get_resolution,
    .reset = MT6816_reset,
};

// 静态实例：[INT_ENCODER] / [EXT_ENCODER]
static tMT6816 s_inst[2] = {
    {.ops = &MT6816_driver_ops, .cmd_high = MT6816_CMD_HIGH, .cmd_low = MT6816_CMD_LOW, .resolution = MT6816_RESOLUTION},
    {.ops = &MT6816_driver_ops, .cmd_high = MT6816_CMD_HIGH, .cmd_low = MT6816_CMD_LOW, .resolution = MT6816_RESOLUTION},
};

EncoderChipHandle MT6816_get_handle(eEncoderType type)
{
    if (type != INT_ENCODER && type != EXT_ENCODER)
        return NULL;
    s_inst[type].type = type;
    s_inst[type].engine = enc_engine_get_handle();
    return (EncoderChipHandle)&s_inst[type];
}

// ---- ops 实现 ----

static bool MT6816_init(EncoderChipHandle h, eEncoderType type)
{
    tMT6816 *ctx = (tMT6816 *)h;
    if (!ctx)
        return false;

    if (!enc_spi_set_mode(ctx->engine, 1U, 1U, 16U)) // 芯片协议：Mode3/16bit
        return false;

    ctx->type = type;
    ctx->raw = 0U;
    return true;
}

static bool MT6816_read_angle(EncoderChipHandle h, uint16_t *raw, uint32_t *ts_ms)
{
    tMT6816 *ctx = (tMT6816 *)h;
    if (!ctx || !raw || !ts_ms)
        return false;

    tEncXferSeg segs[2];
    segs[0].tx = (const uint8_t *)&ctx->cmd_high;
    segs[0].rx = ctx->rx1;
    segs[0].len = 2U;
    segs[1].tx = (const uint8_t *)&ctx->cmd_low;
    segs[1].rx = ctx->rx2;
    segs[1].len = 2U;

    if (!enc_engine_read(ctx->engine, segs, ctx->type, 2U, ts_ms))
        return false;

    uint16_t high = (uint16_t)(ctx->rx1[0] | ((uint16_t)ctx->rx1[1] << 8));
    uint16_t low = (uint16_t)(ctx->rx2[0] | ((uint16_t)ctx->rx2[1] << 8));

    if (low & MT6816_MAG_WARN) // 磁场告警 → 数据不可信
        return false;

    uint16_t bits = (uint16_t)(((high & 0x00FFU) << 7) | ((low & 0x00FEU) >> 1));
    bool parity = (low & MT6816_PARITY_BIT) != 0U;
    if (parity_odd(bits) != parity)
        return false;

    ctx->raw = (uint16_t)(bits >> 1);
    *raw = ctx->raw;
    return true;
}

static bool MT6816_get_resolution(EncoderChipHandle h, uint16_t *res)
{
    tMT6816 *ctx = (tMT6816 *)h;
    if (!ctx || !res)
        return false;
    *res = ctx->resolution;
    return true;
}

static void MT6816_reset(EncoderChipHandle h)
{
    tMT6816 *ctx = (tMT6816 *)h;
    if (!ctx)
        return;
    enc_engine_abort(ctx->engine, ctx->type);
    ctx->raw = 0U;
}
