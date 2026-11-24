#ifndef __BSP_ENC_SPI_H
#define __BSP_ENC_SPI_H

#include <stdbool.h>
#include <stdint.h>

#include "main.h"

// SPI 编码器链路层：SPI1 + DMA 的异步单事务接口。
// 本层不包含芯片协议，三个芯片驱动各自实现 tEncoderOps。

typedef enum {
    BSP_ENC_SPI_MODE_0 = 0,
    BSP_ENC_SPI_MODE_1,
    BSP_ENC_SPI_MODE_2,
    BSP_ENC_SPI_MODE_3,
} tBspEncSpiMode;

typedef enum {
    BSP_ENC_SPI_DATA_8BIT = 0,
    BSP_ENC_SPI_DATA_16BIT,
} tBspEncSpiDataBits;

typedef enum {
    BSP_ENC_SPI_OK = 0,
    BSP_ENC_SPI_ERR_DMA,
    BSP_ENC_SPI_ERR_SPI,
} tBspEncSpiResult;

typedef void (*tBspEncSpiCallback)(tBspEncSpiResult result, void *context);

typedef struct {
    GPIO_TypeDef *cs_port;
    uint32_t cs_pin;
    tBspEncSpiMode mode;
    tBspEncSpiDataBits data_bits;
    tBspEncSpiCallback callback;
    void *context;
} tBspEncSpiConfig;

// 初始化 SPI1/DMA/CS 并注册完成回调。一次只能配置一路编码器。
bool bsp_enc_spi_init(const tBspEncSpiConfig *config);

// 启动一次完整 CS 事务（CS 拉低 → DMA 全双工 → 完成后回调中 CS 抬高）。
// frame_count 为数据项个数：8-bit 模式为字节数，16-bit 模式为 halfword 数。
bool bsp_enc_spi_start(const void *tx_buf, void *rx_buf, uint16_t frame_count);

bool bsp_enc_spi_is_busy(void);

// 中断入口，由 stm32g4xx_it.c 调用。
void bsp_enc_spi_dma_rx_irq(void);
void bsp_enc_spi_dma_tx_irq(void);
void bsp_enc_spi_spi_irq(void);

#endif // __BSP_ENC_SPI_H
