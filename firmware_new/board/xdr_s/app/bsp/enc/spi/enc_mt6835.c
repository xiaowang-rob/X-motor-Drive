// MT6835：Mode3/8bit，连续 5 字节 CS 帧；保持输出高 14 位角度。
#include "bsp_enc_spi.h"

static bool mt6835_read(tSpiEncDev *dev, uint16_t *raw) {
    const uint8_t tx[5] = {0xA0U, 0x03U, 0x00U, 0x00U, 0x00U};
    uint8_t rx[5];
    if (!enc_spi_transfer(dev, tx, rx, sizeof(rx)))
        return false;
    if ((rx[4] & 0x02U) != 0U) // 磁场过弱
        return false;

    uint32_t angle = ((uint32_t)rx[2] << 13) | ((uint32_t)rx[3] << 5) |
                     ((uint32_t)rx[4] >> 3);
    *raw = (uint16_t)(angle >> 7);
    return true;
}

const tSpiEncChipOps spi_mt6835_ops = {
    .cpol = 1U,
    .cpha = 1U,
    .data_bits = 8U,
    .resolution = 16384U,
    .read = mt6835_read,
};
