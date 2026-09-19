// ============================================================
// enc_mt6835.c — MT6835 编码器驱动（板级，SPI 同步读角）
//
// 芯片协议：SPI Mode3(CPOL=1,CPHA=1)/8bit，分辨率 16384
// 读角序列：单段 5 字节 {0xA0,0x03,0,0,0} 边发边收
//   21bit 角度 = rx[2]<<13 | rx[3]<<5 | rx[4]>>3，输出取高 14 位
//   status（rx[4] bit0..2）bit1 = 磁场太弱 → 数据不可信
// 实例形态：按内/外编码器固定的静态实例（无堆分配、无 create/destroy），
//           每实例含 ops / 引擎 / 配置 / 运行时缓冲。
// ============================================================
#include "encoder_drivers.h"

// ---------- 本板配置 ----------
#define MT6835_RESOLUTION 16384U
#define MT6835_FRAME_LEN 5U
#define MT6835_MAG_WEAK_BIT 0x02U

static const uint8_t MT6835_CMD[MT6835_FRAME_LEN] = {0xA0U, 0x03U, 0x00U, 0x00U, 0x00U};

// ---------- 实例 handle ----------
typedef struct
{
    EncEngineHandle engine;       // SPI 引擎（内/外共用）
    eEncoderType type;            // 编码器类型：内/外
    uint16_t resolution;          // 配置：单圈分辨率
    uint8_t rx[MT6835_FRAME_LEN]; // 运行时：接收缓冲
    uint16_t raw;
} tMT6835;

static bool MT6835_init(EncoderChipHandle h, eEncoderType type);
static bool MT6835_read_angle(EncoderChipHandle h, uint16_t *raw, uint32_t *ts_ms);
static bool MT6835_get_resolution(EncoderChipHandle h, uint16_t *res);
static void MT6835_reset(EncoderChipHandle h);

const tEncoderDriverOps MT6835_driver_ops = {
    .init = MT6835_init,
    .read_angle = MT6835_read_angle,
    .get_resolution = MT6835_get_resolution,
    .reset = MT6835_reset,
};

// 编码器芯片可配置 动态分配
EncoderChipHandle MT6835_register_handle(void)
{
    tMT6835 *h = (tMT6835 *)calloc(1, sizeof(tMT6835));
    if (!h)
        return NULL;
    return (EncoderChipHandle)h;
}
// 注销 handle
void MT6835_unregister_handle(EncoderChipHandle h)
{
    tMT6835 *ctx = (tMT6835 *)h;
    if (!ctx)
        return;
    free(ctx);
    ctx = NULL;
}

// ---- ops 实现 ----

static bool MT6835_init(EncoderChipHandle h, eEncoderType type)
{
    tMT6835 *ctx = (tMT6835 *)h;
    if (!ctx)
        return false;
    ctx->type = type;
    ctx->engine = enc_engine_get_handle();
    if (!enc_spi_set_mode(ctx->engine, 1U, 1U, 8U)) // 芯片协议：Mode3/8bit
        return false;

    ctx->raw = 0U;
    return true;
}

static bool MT6835_read_angle(EncoderChipHandle h, uint16_t *raw, uint32_t *ts_ms)
{
    tMT6835 *ctx = (tMT6835 *)h;
    if (!ctx || !raw || !ts_ms)
        return false;

    tEncXferSeg seg;
    seg.tx = MT6835_CMD;
    seg.rx = ctx->rx;
    seg.len = MT6835_FRAME_LEN;

    if (!enc_engine_read(ctx->engine, &seg, ctx->type, 1U, ts_ms))
        return false;

    if (ctx->rx[4] & MT6835_MAG_WEAK_BIT) // 磁场太弱 → 数据不可信
        return false;

    uint32_t angle_21 = ((uint32_t)ctx->rx[2] << 13) |
                        ((uint32_t)ctx->rx[3] << 5) |
                        ((uint32_t)ctx->rx[4] >> 3);

    ctx->raw = (uint16_t)(angle_21 >> 7);
    *raw = ctx->raw;
    return true;
}

static bool MT6835_get_resolution(EncoderChipHandle h, uint16_t *res)
{
    tMT6835 *ctx = (tMT6835 *)h;
    if (!ctx || !res)
        return false;
    *res = ctx->resolution;
    return true;
}

static void MT6835_reset(EncoderChipHandle h)
{
    tMT6835 *ctx = (tMT6835 *)h;
    if (!ctx)
        return;
    enc_engine_abort(ctx->engine, ctx->type);
    ctx->raw = 0U;
}
