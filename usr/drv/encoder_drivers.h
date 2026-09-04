#ifndef __ENCODER_DRIVERS_H
#define __ENCODER_DRIVERS_H

#include "encoder.h"

// ============================================================
// encoder_drivers.h — 编码器芯片驱动统一出口
//
// 板上编码器为固定唯一实例，由 dev_board 装配一个。
//
// 还包含 SPI 编码器芯片驱动函数，用于统一 SPI 读角时序。
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

// 一段全双工传输：tx/rx 为连续内存视图，len 以字节计
// （16bit 模式按 half-word 单元解释，调用方按芯片位宽构造内存视图）
typedef struct
{
    const uint8_t *tx;
    uint8_t *rx;
    uint16_t len;
} tEncXferSeg;

// 按芯片协议配置编码器 SPI（模式/位宽），切换芯片时须重新调用
bool enc_spi_set_mode(uint8_t cpol, uint8_t cpha, uint8_t data_bits);

// 执行一次读角序列（CS 自管理）；任一段失败立即抬 CS 返回 false
bool enc_engine_read(const tEncXferSeg *segs, eEncoderType type, uint8_t n);

// 中止并复位总线（抬 CS）
void enc_engine_abort(eEncoderType type);

// 时间戳（ms，经 platform）
uint32_t enc_tick_ms(void);

#endif // __ENCODER_DRIVERS_H
