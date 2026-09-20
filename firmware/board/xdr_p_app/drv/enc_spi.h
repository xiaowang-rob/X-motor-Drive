#ifndef __ENC_SPI_H
#define __ENC_SPI_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

// ============================================================
// enc_spi.h — 编码器 SPI 读角引擎（板级）
//
// 本板编码器 SPI 单例（hspi3）+ 两路 CS。芯片驱动（enc_mt6816 /
// enc_mt6835 / enc_as5047）都经此收发，不直接触碰 HAL SPI / 引脚。
//
// CS 引脚只在本文件的表中出现，按 tEncCs 暴露；编码器实例各持一份
// 指向自己的 CS，因此同一芯片驱动可挂多路。
// ============================================================

// 编码器路（板级索引）
#define ENC_PATH_EXT 0U // 外接编码器
#define ENC_PATH_INT 1U // 板载编码器
#define ENC_PATH_NUM 2U

// CS 描述（片选：端口 + 引脚）
typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} tEncCs;

// 本板两路 CS
extern const tEncCs g_enc_cs[ENC_PATH_NUM];

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
bool enc_spi_transfer(const tEncXferSeg *segs, const tEncCs *cs, uint8_t n, uint32_t *ts_ms);

// 中止并复位总线（抬 CS）
void enc_spi_abort(const tEncCs *cs);

#endif // __ENC_SPI_H
