#ifndef __BSP_SPI_ENCS_H
#define __BSP_SPI_ENCS_H

#include "encoder.h"
#include "parameter_list.h"

#include "gpio.h"

// spi encoder handle
tEncDevHandle *Get_SPI_ENCHandle(eSpiEncList chip);
// 内置 encoder
tEncDevHandle *Get_INT_ENCHandle(void);

// 芯片协议层：只描述模式、分辨率和一次同步读角，不持有物理实例。
// read 在公共驱动独占总线且已配置模式后调用。
typedef struct {
    uint8_t cpol;
    uint8_t cpha;
    uint8_t data_bits;
    uint16_t resolution;
    bool (*read)(tSpiEncDev *dev, uint16_t *raw);
} tSpiEncChipOps;

typedef struct SPI_ENCODER_DEVICE {
    struct {
        GPIO_TypeDef *port;
        uint32_t pin;
    } cs;
    eSpiEncList chip;
    const tSpiEncChipOps *chip_ops;
    float rad_per_lsb;
    bool inited;
} tSpiEncDev;

extern const tSpiEncChipOps spi_mt6816_ops;
extern const tSpiEncChipOps spi_mt6835_ops;
extern const tSpiEncChipOps spi_as5047_ops;

// 单次 CS 帧：拉低 CS → 全双工收发 → 等待总线空闲 → 拉高 CS。
// len 为字节数；16 位模式使用低字节在前的内存视图，不要求缓冲区对齐。
// 仅供 chip ops 调用，多帧协议通过多次调用产生 CS 边界。
bool enc_spi_transfer(tSpiEncDev *dev, const uint8_t *tx, uint8_t *rx,
                      uint16_t len);

#endif // __BSP_SPI_ENCS_H
