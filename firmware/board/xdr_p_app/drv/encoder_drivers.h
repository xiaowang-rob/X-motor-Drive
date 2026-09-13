#ifndef __ENCODER_DRIVERS_H
#define __ENCODER_DRIVERS_H

#include "encoder.h"

// ============================================================
// encoder_drivers.h — 本板编码器驱动出口
//
//   SPI 读角引擎（单 SPI 总线，内/外编码器共用）—— enc_engine_*
//   芯片驱动（AS5047 / MT6816 / MT6835）—— 静态实例，按内/外类型取用
//
// 实例获取：xxx_get_handle(eEncoderType type)，无堆分配、无 create/destroy。
// ============================================================

// ---- SPI 读角引擎 ----
typedef void *EncEngineHandle;

// 取引擎实例句柄（静态实例，见 encoder_drivers.c）
EncEngineHandle enc_engine_get_handle(void);

// 一段全双工传输：tx/rx 为连续内存视图，len 以字节计
// （16bit 模式按 half-word 单元解释，调用方按芯片位宽构造内存视图）
typedef struct
{
    const uint8_t *tx;
    uint8_t *rx;
    uint16_t len;
} tEncXferSeg;

// 按芯片协议配置编码器 SPI（模式/位宽），切换芯片时须重新调用
bool enc_spi_set_mode(EncEngineHandle e, uint8_t cpol, uint8_t cpha, uint8_t data_bits);

// 执行一次读角序列（CS 自管理）；任一段失败立即抬 CS 返回 false
bool enc_engine_read(EncEngineHandle e, const tEncXferSeg *segs, eEncoderType type,
                     uint8_t n, uint32_t *ms);

// 中止并复位总线（抬 CS）
void enc_engine_abort(EncEngineHandle e, eEncoderType type);

// ---- 芯片驱动（内/外各一个静态实例；type 越界返回 NULL） ----
extern const tEncoderDriverOps AS5047_driver_ops;
extern const tEncoderDriverOps MT6816_driver_ops;
extern const tEncoderDriverOps MT6835_driver_ops;

EncoderChipHandle AS5047_get_handle(eEncoderType type);
EncoderChipHandle MT6816_get_handle(eEncoderType type);
EncoderChipHandle MT6835_get_handle(eEncoderType type);

#endif // __ENCODER_DRIVERS_H
