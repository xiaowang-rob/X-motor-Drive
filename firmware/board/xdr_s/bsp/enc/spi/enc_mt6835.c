// MT6835：Mode3/8bit，一次连续 5 字节 CS 事务。
// 本驱动直接基于 bsp_enc_spi 的 DMA 异步事务。

#include "enc_mt6835.h"

#include "bsp_enc_spi.h"
#include "bsp_mcu.h"
#include "xmath.h"

#define MT6835_RESOLUTION 16384U

typedef enum {
    MT6835_PHASE_IDLE = 0,
    MT6835_PHASE_FRAME,
} tMt6835Phase;

typedef struct {
    GPIO_TypeDef *cs_port;
    uint32_t cs_pin;
    bool initialized;

    volatile tMt6835Phase phase;
    uint8_t tx[5];
    uint8_t rx[5];

    tEncSample latest;
    volatile bool latest_valid;
    volatile uint32_t error_count;
} tMt6835Dev;

static bool mt6835_init(void *handle);
static bool mt6835_open(void *handle);
static bool mt6835_read(void *handle, tEncSample *sample);
static bool mt6835_close(void *handle);

static const tEncoderOps mt6835_ops = {
    .init = mt6835_init,
    .open = mt6835_open,
    .read = mt6835_read,
    .close = mt6835_close,
};

static tMt6835Dev s_ext = {
    .cs_port = EXTCS_GPIO_Port,
    .cs_pin = EXTCS_Pin,
};

static const tEncDevHandle s_ext_handle = {
    .ops = &mt6835_ops,
    .dev = &s_ext,
};

const tEncDevHandle *enc_mt6835_ext_handle(void) { return &s_ext_handle; }

static void mt6835_callback(tBspEncSpiResult result, void *context) {
    tMt6835Dev *dev = context;
    if (!dev || dev->phase != MT6835_PHASE_FRAME)
        return;

    dev->phase = MT6835_PHASE_IDLE;

    if (result != BSP_ENC_SPI_OK || (dev->rx[4] & 0x02U) != 0U) {
        dev->error_count++;
        return;
    }

    uint32_t angle = ((uint32_t)dev->rx[2] << 13) |
                     ((uint32_t)dev->rx[3] << 5) | ((uint32_t)dev->rx[4] >> 3);
    uint16_t raw = (uint16_t)(angle >> 7);

    tEncSample sample = {
        .raw = raw,
        .resolution = MT6835_RESOLUTION,
        .ts_us = bsp_time_us(),
        .angle_rad = (float)raw * (XMATH_2PI / (float)MT6835_RESOLUTION),
        .angle_kind = BSP_ANGLE_MECHANICAL,
        .referenced = true,
    };
    dev->latest = sample;
    dev->latest_valid = true;
}

static bool mt6835_init(void *handle) {
    tMt6835Dev *dev = handle;
    if (!dev)
        return false;
    dev->initialized = false;
    dev->phase = MT6835_PHASE_IDLE;
    dev->latest_valid = false;
    dev->error_count = 0;
    return true;
}

static bool mt6835_open(void *handle) {
    tMt6835Dev *dev = handle;
    if (!dev)
        return false;

    tBspEncSpiConfig config = {
        .cs_port = dev->cs_port,
        .cs_pin = dev->cs_pin,
        .mode = BSP_ENC_SPI_MODE_3,
        .data_bits = BSP_ENC_SPI_DATA_8BIT,
        .callback = mt6835_callback,
        .context = dev,
    };
    if (!bsp_enc_spi_init(&config))
        return false;

    dev->initialized = true;
    dev->phase = MT6835_PHASE_IDLE;
    dev->latest_valid = false;
    dev->error_count = 0;
    return true;
}

static bool mt6835_read(void *handle, tEncSample *sample) {
    tMt6835Dev *dev = handle;
    if (!dev || !dev->initialized || !sample)
        return false;

    if (dev->phase == MT6835_PHASE_IDLE && !bsp_enc_spi_is_busy()) {
        dev->tx[0] = 0xA0U;
        dev->tx[1] = 0x03U;
        dev->tx[2] = 0x00U;
        dev->tx[3] = 0x00U;
        dev->tx[4] = 0x00U;
        dev->rx[4] = 0U;

        dev->phase = MT6835_PHASE_FRAME;
        if (!bsp_enc_spi_start(dev->tx, dev->rx, 5U)) {
            dev->phase = MT6835_PHASE_IDLE;
            dev->error_count++;
        }
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    bool valid = dev->latest_valid;
    if (valid)
        *sample = dev->latest;
    __set_PRIMASK(primask);
    return valid;
}

static bool mt6835_close(void *handle) {
    tMt6835Dev *dev = handle;
    if (!dev)
        return false;
    dev->initialized = false;
    dev->phase = MT6835_PHASE_IDLE;
    dev->latest_valid = false;
    return true;
}
