#ifndef __ENCODER_DRIVERS_H
#define __ENCODER_DRIVERS_H

#include "usr/abs/encoder.h"

// ============================================================
// encoder_drivers.h — 编码器芯片驱动统一出口（usr/drv，v2 直连版）
//
// v2：驱动直接使用本板外设（经 platform.h），create 无参；
// 板上编码器为固定唯一实例，由 dev_board 装配一个。
// ============================================================

// ---- AS5047（SPI Mode1 / 16bit / 14bit，16384） ----
EncoderChipHandle AS5047_create(void);
void AS5047_destroy(EncoderChipHandle h);
extern const tEncoderDriverOps AS5047_driver_ops;

// ---- MT6816（SPI Mode3 / 16bit / 14bit，16384） ----
EncoderChipHandle MT6816_create(void);
void MT6816_destroy(EncoderChipHandle h);
extern const tEncoderDriverOps MT6816_driver_ops;

// ---- MT6835（SPI Mode3 / 8bit / 14bit，16384） ----
EncoderChipHandle MT6835_create(void);
void MT6835_destroy(EncoderChipHandle h);
extern const tEncoderDriverOps MT6835_driver_ops;

#endif // __ENCODER_DRIVERS_H
