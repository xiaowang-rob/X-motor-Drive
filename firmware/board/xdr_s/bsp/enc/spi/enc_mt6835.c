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

static bool mt6835_init(void *dev, void *cfg);
static bool mt6835_open(void *dev);
static bool mt6835_read(void *dev, tEncSample *sample);
static bool mt6835_close(void *dev);

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

const tEncDevHandle *Get_EncMT6835Handle(void) { return &s_ext_handle; }

static void mt6835_callback(tBspEncSpiResult result, void *context) {
    tMt6835Dev *inst = context;
    if (!inst || inst->phase != MT6835_PHASE_FRAME)
        return;

    inst->phase = MT6835_PHASE_IDLE;

    if (result != BSP_ENC_SPI_OK || (inst->rx[4] & 0x02U) != 0U) {
        inst->error_count++;
        return;
    }

    uint32_t angle = ((uint32_t)inst->rx[2] << 13) |
                     ((uint32_t)inst->rx[3] << 5) |
                     ((uint32_t)inst->rx[4] >> 3);
    uint16_t raw = (uint16_t)(angle >> 7);

    tEncSample sample = {
        .ts_us = bsp_time_us(),
        .angle_rad = (float)raw * (XMATH_2PI / (float)MT6835_RESOLUTION),
        .angle_kind = BSP_ANGLE_MECHANICAL,
        .referenced = true,
    };
    inst->latest = sample;
    inst->latest_valid = true;
}

static bool mt6835_init(void *dev, void *cfg) {
    tMt6835Dev *inst = dev;
    if (!inst)
        return false;
    inst->initialized = false;
    inst->phase = MT6835_PHASE_IDLE;
    inst->latest_valid = false;
    inst->error_count = 0;
    return true;
}

static bool mt6835_open(void *dev) {
    tMt6835Dev *inst = dev;
    if (!inst)
        return false;

    tBspEncSpiConfig config = {
        .cs_port = inst->cs_port,
        .cs_pin = inst->cs_pin,
        .mode = BSP_ENC_SPI_MODE_3,
        .data_bits = BSP_ENC_SPI_DATA_8BIT,
        .callback = mt6835_callback,
        .context = inst,
    };
    if (!bsp_enc_spi_init(&config))
        return false;

    inst->initialized = true;
    inst->phase = MT6835_PHASE_IDLE;
    inst->latest_valid = false;
    inst->error_count = 0;
    return true;
}

static bool mt6835_read(void *dev, tEncSample *sample) {
    tMt6835Dev *inst = dev;
    if (!inst || !inst->initialized || !sample)
        return false;

    if (inst->phase == MT6835_PHASE_IDLE && !bsp_enc_spi_is_busy()) {
        inst->tx[0] = 0xA0U;
        inst->tx[1] = 0x03U;
        inst->tx[2] = 0x00U;
        inst->tx[3] = 0x00U;
        inst->tx[4] = 0x00U;
        inst->rx[4] = 0U;

        inst->phase = MT6835_PHASE_FRAME;
        if (!bsp_enc_spi_start(inst->tx, inst->rx, 5U)) {
            inst->phase = MT6835_PHASE_IDLE;
            inst->error_count++;
        }
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    bool valid = inst->latest_valid;
    if (valid)
        *sample = inst->latest;
    __set_PRIMASK(primask);
    return valid;
}

static bool mt6835_close(void *dev) {
    tMt6835Dev *inst = dev;
    if (!inst)
        return false;
    inst->initialized = false;
    inst->phase = MT6835_PHASE_IDLE;
    inst->latest_valid = false;
    return true;
}
