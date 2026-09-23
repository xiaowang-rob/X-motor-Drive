// ============================================================
// enc_as5047.c — AS5047 编码器驱动（板级，SPI 同步读角）
//
// 芯片协议：SPI Mode1(CPOL=0,CPHA=1)/16bit，分辨率 16384
// 读角序列：段1 发 0x7FFF（弃响应）→ 段2 发 0x0000 收角度帧
// 角度帧：bit14 错误标志；bit13..0 角度
// ============================================================
#include "enc_as5047.h"

#include "enc_spi.h"

// ---------- 芯片协议常量 ----------

// AS5047：SPI Mode1(CPOL=0,CPHA=1)/16bit，分辨率 16384
// 读角序列：段1 发 0x7FFF（弃响应）→ 段2 发 0x0000 收角度帧
#define AS5047_RESOLUTION 16384U

#define AS5047_CMD_READ 0x7FFFU
#define AS5047_CMD_NOP 0x0000U
#define AS5047_ERR_FLAG 0x4000U // 角度帧 bit14
#define AS5047_ANGLE_MASK 0x3FFFU

// ---------- 实例（每路一份，CS 由本路持有） ----------
struct tAS5047Dev
{
    bool inited;       // open 后置位
    eENCtype type;     // 类型
    uint16_t cmd_read; // 段1 tx
    uint16_t cmd_nop;  // 段2 tx
    uint8_t rx1[2];    // 段1 rx（弃用）
    uint8_t rx2[2];    // 段2 rx（角度帧）
};

tAS5047Dev g_as5047_ext = {.type = ENC_EXT};
tAS5047Dev g_as5047_int = {.type = ENC_INT};

// ---- 芯片接口 ----

static bool as5047_open(void *handle, uint16_t *resolution)
{
    tAS5047Dev *d = (tAS5047Dev *)handle;
    if (!d || !resolution)
        return false;

    if (!enc_spi_ensure_mode(0U, 1U, 16U)) // 芯片协议：Mode1/16bit
        return false;

    d->cmd_read = AS5047_CMD_READ;
    d->cmd_nop = AS5047_CMD_NOP;
    d->inited = true;

    *resolution = AS5047_RESOLUTION;
    return true;
}

static bool as5047_read(void *handle, uint16_t *raw, uint32_t *ts_ms)
{
    tAS5047Dev *d = (tAS5047Dev *)handle;
    if (!d || !raw || !ts_ms || !d->inited)
        return false;

    if (!enc_spi_ensure_mode(0U, 1U, 16U))
        return false;

    tEncXferSeg segs[2];
    segs[0].tx = (const uint8_t *)&d->cmd_read;
    segs[0].rx = d->rx1;
    segs[0].len = 2U;
    segs[1].tx = (const uint8_t *)&d->cmd_nop;
    segs[1].rx = d->rx2;
    segs[1].len = 2U;

    if (!enc_spi_transfer(segs, d->type, 2U, ts_ms))
        return false;

    uint16_t frame = (uint16_t)(d->rx2[0] | ((uint16_t)d->rx2[1] << 8));
    if (frame & AS5047_ERR_FLAG)
        return false;

    *raw = (uint16_t)(frame & AS5047_ANGLE_MASK);
    return true;
}

static void as5047_abort(void *handle)
{
    tAS5047Dev *d = (tAS5047Dev *)handle;
    if (!d)
        return;
    enc_spi_abort(d->type);
}

// ---- 驱动出口 ----
const tEncoderOps as5047_ops = {
    .open = as5047_open,
    .read = as5047_read,
    .abort = as5047_abort,
};
