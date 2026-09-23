#ifndef __ENC_SPI_H
#define __ENC_SPI_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32g4xx_hal.h"

typedef enum
{
    ENC_INT,
    ENC_EXT,
} eENCtype;

// 一段全双工传输：tx/rx 为连续内存视图，len 以字节计
// （16bit 模式按 half-word 单元解释，调用方按芯片位宽构造内存视图）
typedef struct
{
    const uint8_t *tx;
    uint8_t *rx;
    uint16_t len;
} tEncXferSeg;

// 幂等设置 SPI 模式：已处于目标模式则不重配。
// 内外两颗芯片协议不同（如 MT6816 16bit / MT6835 8bit）时，
// 交替读角必须靠它保证总线模式正确。
bool enc_spi_ensure_mode(uint8_t cpol, uint8_t cpha, uint8_t data_bits);

// 执行一次读角序列（CS 自管理）；任一段失败立即抬 CS 返回 false
bool enc_spi_transfer(const tEncXferSeg *segs, eENCtype type, uint8_t n, uint32_t *ts_ms);

// 中止并复位总线（抬 CS）
void enc_spi_abort(eENCtype type);

#endif // __ENC_SPI_H
