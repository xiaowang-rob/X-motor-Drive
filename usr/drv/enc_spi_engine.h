#ifndef __ENC_SPI_ENGINE_H
#define __ENC_SPI_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// enc_spi_engine.h — 同步磁编码器 SPI 读角时序工具（usr/drv，v2 直连版）
//
// v2：本模块直接使用本板编码器 SPI（platform.h 的 ENCODER_SPI_CH）
// 与片选引脚，统一完成"CS 拉低 → 逐段同步收发 → CS 拉高"。
// 芯片差异（SPI 模式、段内容、解析）留在各芯片文件。
// ============================================================

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
bool enc_engine_read(const tEncXferSeg *segs, uint8_t n);

// 中止并复位总线（抬 CS）
void enc_engine_abort(void);

// 时间戳（ms，经 platform）
uint32_t enc_tick_ms(void);

#endif // __ENC_SPI_ENGINE_H
