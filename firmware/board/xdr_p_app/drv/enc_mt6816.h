#ifndef XDR_BOARD_DRV_ENC_MT6816_H
#define XDR_BOARD_DRV_ENC_MT6816_H

#include <stdint.h>

#include "encoder.h"

// MT6816：SPI Mode3(CPOL=1,CPHA=1)/16bit，分辨率 16384
// 读角序列：段1 发 0x83FF → 段2 发 0x84FF 收有效数据帧
#define MT6816_RESOLUTION 16384U

bool mt6816_open(eEncoderType type, uint16_t *resolution);
bool mt6816_read(eEncoderType type, uint16_t *raw, uint32_t *ts_ms);
void mt6816_abort(eEncoderType type);

#endif // XDR_BOARD_DRV_ENC_MT6816_H
