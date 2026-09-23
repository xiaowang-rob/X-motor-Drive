// ============================================================
// enc_mt6835.c — MT6835 编码器驱动（板级，SPI 同步读角）
//
// 芯片协议：SPI Mode3(CPOL=1,CPHA=1)/8bit，分辨率 16384
// 读角序列：单段 5 字节 {0xA0,0x03,0,0,0} 边发边收
//   21bit 角度 = rx[2]<<13 | rx[3]<<5 | rx[4]>>3，输出取高 14 位
//   status（rx[4] bit0..2）bit1 = 磁场太弱 → 数据不可信
//
// 实现 abs/encoder.h 的 tEncoderOps；实例持本路 CS（见 enc_spi.h）。
// ============================================================
#include "enc_mt6835.h"

#include "enc_spi.h"

// ---------- 芯片协议常量 ----------
#define MT6835_FRAME_LEN 5U
#define MT6835_MAG_WEAK_BIT 0x02U // status.bit1：磁场太弱

static const uint8_t MT6835_CMD[MT6835_FRAME_LEN] = {0xA0U, 0x03U, 0x00U, 0x00U, 0x00U};

// ---------- 实例（每路一份，CS 由本路持有） ----------
struct tMT6835Dev
{
    bool inited;                  // open 后置位
    const tEncCs *cs;             // 本路 CS
    uint8_t rx[MT6835_FRAME_LEN]; // 接收缓冲
};

tMT6835Dev g_mt6835_ext = {.cs = &g_enc_cs[ENC_PATH_EXT]};
tMT6835Dev g_mt6835_int = {.cs = &g_enc_cs[ENC_PATH_INT]};

// ---- 芯片接口 ----

static bool mt6835_open(void *handle, uint16_t *resolution)
{
    tMT6835Dev *d = (tMT6835Dev *)handle;
    if (!d || !d->cs || !resolution)
        return false;

    if (!enc_spi_ensure_mode(1U, 1U, 8U)) // 芯片协议：Mode3/8bit
        return false;

    d->inited = true;
    *resolution = MT6835_RESOLUTION;
    return true;
}

static bool mt6835_read(void *handle, uint16_t *raw, uint32_t *ts_ms)
{
    tMT6835Dev *d = (tMT6835Dev *)handle;
    if (!d || !d->cs || !raw || !ts_ms || !d->inited)
        return false;

    if (!enc_spi_ensure_mode(1U, 1U, 8U))
        return false;

    tEncXferSeg seg;
    seg.tx = MT6835_CMD;
    seg.rx = d->rx;
    seg.len = MT6835_FRAME_LEN;

    if (!enc_spi_transfer(&seg, d->cs, 1U, ts_ms))
        return false;

    if (d->rx[4] & MT6835_MAG_WEAK_BIT) // 磁场太弱 → 数据不可信
        return false;

    uint32_t angle_21 = ((uint32_t)d->rx[2] << 13) |
                        ((uint32_t)d->rx[3] << 5) |
                        ((uint32_t)d->rx[4] >> 3);

    *raw = (uint16_t)(angle_21 >> 7); // 取高 14 位
    return true;
}

static void mt6835_abort(void *handle)
{
    tMT6835Dev *d = (tMT6835Dev *)handle;
    if (!d)
        return;
    enc_spi_abort(d->cs);
}

// ---- 驱动出口 ----
const tEncoderOps mt6835_ops = {
    .open = mt6835_open,
    .read = mt6835_read,
    .abort = mt6835_abort,
};
