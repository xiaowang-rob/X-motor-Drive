// ============================================================
// enc_as5047.c — AS5047 编码器驱动（板级，SPI 同步读角）
//
// 芯片协议：SPI Mode1(CPOL=0,CPHA=1)/16bit，分辨率 16384
// 读角序列：段1 发 0x7FFF（弃响应）→ 段2 发 0x0000 收角度帧
// 角度帧：bit14 错误标志；bit13..0 角度
//
// 实例形态：内/外各一份文件内静态实例（无堆分配、无 create/destroy）。
// ============================================================
#include "enc_as5047.h"

#include "enc_spi.h"

// ---------- 芯片协议常量 ----------
#define AS5047_CMD_READ 0x7FFFU
#define AS5047_CMD_NOP 0x0000U
#define AS5047_ERR_FLAG 0x4000U  // 角度帧 bit14
#define AS5047_ANGLE_MASK 0x3FFFU

// ---------- 实例 ----------
typedef struct
{
    bool inited;      // open 后置位
    uint16_t cmd_read; // 段1 tx
    uint16_t cmd_nop;  // 段2 tx
    uint8_t rx1[2];    // 段1 rx（弃用）
    uint8_t rx2[2];    // 段2 rx（角度帧）
} tAS5047Dev;

static tAS5047Dev s_dev[2]; // [EXT_ENCODER] / [INT_ENCODER]

static bool as5047_type_ok(eEncoderType type)
{
    return (unsigned)type <= (unsigned)INT_ENCODER;
}

// ---- 芯片接口 ----

bool as5047_open(eEncoderType type, uint16_t *resolution)
{
    if (!resolution || !as5047_type_ok(type))
        return false;

    if (!enc_spi_ensure_mode(0U, 1U, 16U)) // 芯片协议：Mode1/16bit
        return false;

    tAS5047Dev *d = &s_dev[type];
    d->cmd_read = AS5047_CMD_READ;
    d->cmd_nop = AS5047_CMD_NOP;
    d->inited = true;

    *resolution = AS5047_RESOLUTION;
    return true;
}

bool as5047_read(eEncoderType type, uint16_t *raw, uint32_t *ts_ms)
{
    if (!raw || !ts_ms || !as5047_type_ok(type))
        return false;

    tAS5047Dev *d = &s_dev[type];
    if (!d->inited)
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

    if (!enc_spi_transfer(segs, type, 2U, ts_ms))
        return false;

    uint16_t frame = (uint16_t)(d->rx2[0] | ((uint16_t)d->rx2[1] << 8));
    if (frame & AS5047_ERR_FLAG)
        return false;

    *raw = (uint16_t)(frame & AS5047_ANGLE_MASK);
    return true;
}

void as5047_abort(eEncoderType type)
{
    if (!as5047_type_ok(type))
        return;
    enc_spi_abort(type);
}
