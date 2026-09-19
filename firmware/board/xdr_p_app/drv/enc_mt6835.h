#ifndef XDR_BOARD_DRV_ENC_MT6835_H
#define XDR_BOARD_DRV_ENC_MT6835_H

#include <stdint.h>

#include "encoder.h"

// MT6835：SPI Mode3(CPOL=1,CPHA=1)/8bit，分辨率 16384
// 读角序列：单段 5 字节 {0xA0,0x03,0,0,0} 边发边收
#define MT6835_RESOLUTION 16384U

bool mt6835_open(eEncoderType type, uint16_t *resolution);
bool mt6835_read(eEncoderType type, uint16_t *raw, uint32_t *ts_ms);
void mt6835_abort(eEncoderType type);

#endif // XDR_BOARD_DRV_ENC_MT6835_H
