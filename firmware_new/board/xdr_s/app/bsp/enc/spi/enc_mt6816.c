// MT6816：Mode3/16bit，两次独立 CS 帧读取 0x03、0x04 寄存器。
#include "bsp_enc_spi.h"

static bool parity_odd(uint16_t value) {
    value ^= (uint16_t)(value >> 8);
    value ^= (uint16_t)(value >> 4);
    value ^= (uint16_t)(value >> 2);
    value ^= (uint16_t)(value >> 1);
    return (value & 1U) != 0U;
}

static bool mt6816_read(tSpiEncDev *dev, uint16_t *raw) {
    const uint8_t tx_high[2] = {0xFFU, 0x83U};
    const uint8_t tx_low[2] = {0xFFU, 0x84U};
    uint8_t high[2], low[2];
    if (!enc_spi_transfer(dev, tx_high, high, sizeof(high)) ||
        !enc_spi_transfer(dev, tx_low, low, sizeof(low)))
        return false;

    // 每次 16bit 收发的低字节是寄存器值；angle[13:0], no_mag, parity。
    uint16_t frame = (uint16_t)(((uint16_t)high[0] << 8) | low[0]);
    if ((frame & 0x02U) != 0U || parity_odd(frame))
        return false;

    *raw = (uint16_t)(frame >> 2);
    return true;
}

const tSpiEncChipOps spi_mt6816_ops = {
    .cpol = 1U,
    .cpha = 1U,
    .data_bits = 16U,
    .resolution = 16384U,
    .read = mt6816_read,
};
