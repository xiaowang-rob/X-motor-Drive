// AS5047：Mode1/16bit；读命令与 NOP 各占一个 CS 帧。
#include "bsp_spi_encs.h"

static bool parity_odd(uint16_t value)
{
    value ^= (uint16_t)(value >> 8);
    value ^= (uint16_t)(value >> 4);
    value ^= (uint16_t)(value >> 2);
    value ^= (uint16_t)(value >> 1);
    return (value & 1U) != 0U;
}

static bool as5047_read(tSpiEncDev *dev, uint16_t *raw)
{
    // 0xFFFF：读 0x3FFF，含偶校验位；0x0000：NOP。
    const uint8_t tx_read[2] = {0xFFU, 0xFFU};
    const uint8_t tx_nop[2] = {0x00U, 0x00U};
    uint8_t discard[2], rx[2];
    if (!enc_spi_transfer(dev, tx_read, discard, sizeof(discard)) ||
        !enc_spi_transfer(dev, tx_nop, rx, sizeof(rx)))
        return false;

    uint16_t frame = (uint16_t)(rx[0] | ((uint16_t)rx[1] << 8));
    if ((frame & 0x4000U) != 0U || parity_odd(frame))
        return false;
    *raw = (uint16_t)(frame & 0x3FFFU);
    return true;
}

const tSpiEncChipOps spi_as5047_ops = {
    .cpol = 0U, .cpha = 1U, .data_bits = 16U,
    .resolution = 16384U,
    .read = as5047_read,
};
