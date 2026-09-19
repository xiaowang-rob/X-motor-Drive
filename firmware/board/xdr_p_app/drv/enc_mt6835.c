// ============================================================
// enc_mt6835.c — MT6835 编码器驱动（板级，SPI 同步读角）
//
// 芯片协议：SPI Mode3(CPOL=1,CPHA=1)/8bit，分辨率 16384
// 读角序列：单段 5 字节 {0xA0,0x03,0,0,0} 边发边收
//   21bit 角度 = rx[2]<<13 | rx[3]<<5 | rx[4]>>3，输出取高 14 位
//   status（rx[4] bit0..2）bit1 = 磁场太弱 → 数据不可信
//
// 实例形态：内/外各一份文件内静态实例（无堆分配、无 create/destroy）。
// ============================================================
#include "enc_mt6835.h"

#include "enc_spi.h"

// ---------- 芯片协议常量 ----------
#define MT6835_FRAME_LEN 5U
#define MT6835_MAG_WEAK_BIT 0x02U // status.bit1：磁场太弱

static const uint8_t MT6835_CMD[MT6835_FRAME_LEN] = {0xA0U, 0x03U, 0x00U, 0x00U, 0x00U};

// ---------- 实例 ----------
typedef struct
{
    bool inited;                  // open 后置位
    uint8_t rx[MT6835_FRAME_LEN]; // 接收缓冲
} tMT6835Dev;

static tMT6835Dev s_dev[2]; // [EXT_ENCODER] / [INT_ENCODER]

static bool mt6835_type_ok(eEncoderType type)
{
    return (unsigned)type <= (unsigned)INT_ENCODER;
}

// ---- 芯片接口 ----

bool mt6835_open(eEncoderType type, uint16_t *resolution)
{
    if (!resolution || !mt6835_type_ok(type))
        return false;

    if (!enc_spi_ensure_mode(1U, 1U, 8U)) // 芯片协议：Mode3/8bit
        return false;

    s_dev[type].inited = true;
    *resolution = MT6835_RESOLUTION;
    return true;
}

bool mt6835_read(eEncoderType type, uint16_t *raw, uint32_t *ts_ms)
{
    if (!raw || !ts_ms || !mt6835_type_ok(type))
        return false;

    tMT6835Dev *d = &s_dev[type];
    if (!d->inited)
        return false;

    if (!enc_spi_ensure_mode(1U, 1U, 8U))
        return false;

    tEncXferSeg seg;
    seg.tx = MT6835_CMD;
    seg.rx = d->rx;
    seg.len = MT6835_FRAME_LEN;

    if (!enc_spi_transfer(&seg, type, 1U, ts_ms))
        return false;

    if (d->rx[4] & MT6835_MAG_WEAK_BIT) // 磁场太弱 → 数据不可信
        return false;

    uint32_t angle_21 = ((uint32_t)d->rx[2] << 13) |
                        ((uint32_t)d->rx[3] << 5) |
                        ((uint32_t)d->rx[4] >> 3);

    *raw = (uint16_t)(angle_21 >> 7); // 取高 14 位
    return true;
}

void mt6835_abort(eEncoderType type)
{
    if (!mt6835_type_ok(type))
        return;
    enc_spi_abort(type);
}
