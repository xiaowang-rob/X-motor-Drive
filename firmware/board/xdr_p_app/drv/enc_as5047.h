#ifndef XDR_BOARD_DRV_ENC_AS5047_H
#define XDR_BOARD_DRV_ENC_AS5047_H

#include <stdint.h>

#include "encoder.h"

// AS5047：SPI Mode1(CPOL=0,CPHA=1)/16bit，分辨率 16384
// 读角序列：段1 发 0x7FFF（弃响应）→ 段2 发 0x0000 收角度帧
#define AS5047_RESOLUTION 16384U

bool as5047_open(eEncoderType type, uint16_t *resolution);
bool as5047_read(eEncoderType type, uint16_t *raw, uint32_t *ts_ms);
void as5047_abort(eEncoderType type);

#endif // XDR_BOARD_DRV_ENC_AS5047_H
