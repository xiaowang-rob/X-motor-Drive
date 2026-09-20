// ============================================================
// enc_mt6816.c — MT6816 编码器驱动（板级，SPI 同步读角）
//
// 芯片协议：SPI Mode3(CPOL=1,CPHA=1)/16bit，分辨率 16384
// 读角序列：段1 发 0x83FF → 段2 发 0x84FF 收有效数据帧
// 解析：磁场告警位 + 奇偶校验
//
// 实现 abs/encoder.h 的 tEncoderOps；实例持本路 CS（见 enc_spi.h）。
// ============================================================
#include "enc_mt6816.h"

#include "enc_spi.h"

// ---------- 芯片协议常量 ----------
#define MT6816_CMD_HIGH 0x83FFU
#define MT6816_CMD_LOW 0x84FFU
#define MT6816_MAG_WARN (1U << 1)   // 磁场告警位
#define MT6816_PARITY_BIT (1U << 0) // 奇偶校验位

// ---------- 实例（每路一份，CS 由本路持有） ----------
struct tMT6816Dev
{
    bool inited;       // open 后置位，防止未初始化就读角
    const tEncCs *cs;  // 本路 CS
    uint16_t cmd_high; // 段1 tx
    uint16_t cmd_low;  // 段2 tx
    uint8_t rx1[2];    // 段1 rx
    uint8_t rx2[2];    // 段2 rx
};

tMT6816Dev g_mt6816_ext = {.cs = &g_enc_cs[ENC_PATH_EXT]};
tMT6816Dev g_mt6816_int = {.cs = &g_enc_cs[ENC_PATH_INT]};

static bool parity_odd(uint16_t v)
{
    v ^= (uint16_t)(v >> 8);
    v ^= (uint16_t)(v >> 4);
    v ^= (uint16_t)(v >> 2);
    v ^= (uint16_t)(v >> 1);
    return (v & 1U) != 0U;
}

// ---- 芯片接口 ----

static bool mt6816_open(void *handle, uint16_t *resolution)
{
    tMT6816Dev *d = (tMT6816Dev *)handle;
    if (!d || !d->cs || !resolution)
        return false;

    if (!enc_spi_ensure_mode(1U, 1U, 16U)) // 芯片协议：Mode3/16bit
        return false;

    d->cmd_high = MT6816_CMD_HIGH;
    d->cmd_low = MT6816_CMD_LOW;
    d->inited = true;

    *resolution = MT6816_RESOLUTION;
    return true;
}

static bool mt6816_read(void *handle, uint16_t *raw, uint32_t *ts_ms)
{
    tMT6816Dev *d = (tMT6816Dev *)handle;
    if (!d || !d->cs || !raw || !ts_ms || !d->inited)
        return false;

    // 与另一路芯片交替读角时，总线模式可能被对方改过
    if (!enc_spi_ensure_mode(1U, 1U, 16U))
        return false;

    tEncXferSeg segs[2];
    segs[0].tx = (const uint8_t *)&d->cmd_high;
    segs[0].rx = d->rx1;
    segs[0].len = 2U;
    segs[1].tx = (const uint8_t *)&d->cmd_low;
    segs[1].rx = d->rx2;
    segs[1].len = 2U;

    if (!enc_spi_transfer(segs, d->cs, 2U, ts_ms))
        return false;

    uint16_t high = (uint16_t)(d->rx1[0] | ((uint16_t)d->rx1[1] << 8));
    uint16_t low = (uint16_t)(d->rx2[0] | ((uint16_t)d->rx2[1] << 8));

    if (low & MT6816_MAG_WARN) // 磁场告警 → 数据不可信
        return false;

    uint16_t bits = (uint16_t)(((high & 0x00FFU) << 7) | ((low & 0x00FEU) >> 1));
    bool parity = (low & MT6816_PARITY_BIT) != 0U;
    if (parity_odd(bits) != parity)
        return false;

    *raw = (uint16_t)(bits >> 1);
    return true;
}

static void mt6816_abort(void *handle)
{
    tMT6816Dev *d = (tMT6816Dev *)handle;
    if (!d)
        return;
    enc_spi_abort(d->cs);
}

// ---- 驱动出口 ----
const tEncoderOps mt6816_ops = {
    .open = mt6816_open,
    .read = mt6816_read,
    .abort = mt6816_abort,
};
