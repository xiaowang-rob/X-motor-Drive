// MT6816：Mode3/16bit，两次独立 CS 事务读取 0x03、0x04 寄存器。
// 本驱动直接基于 bsp_enc_spi 的 DMA 异步事务，不依赖通用 SPI 编码器收敛层。

#include "enc_mt6816.h"

#include "bsp_enc_spi.h"
#include "bsp_mcu.h"
#include "xmath.h"

#define MT6816_RESOLUTION 16384U
#define MT6816_CMD_HIGH 0x83ffU
#define MT6816_CMD_LOW 0x84ffU
typedef enum {
    MT6816_PHASE_IDLE = 0,
    MT6816_PHASE_HIGH,
    MT6816_PHASE_LOW,
} tMt6816Phase;

typedef struct {
    GPIO_TypeDef *cs_port;
    uint32_t cs_pin;
    bool initialized;

    volatile tMt6816Phase phase;
    uint16_t tx;
    uint16_t rx;
    uint8_t high;

    tEncSample latest;
    volatile bool latest_valid;
    volatile uint32_t error_count;
} tMt6816Dev;

static bool mt6816_init(void *dev, void *cfg);
static bool mt6816_open(void *dev);
static bool mt6816_read(void *dev, tEncSample *sample);
static bool mt6816_close(void *dev);

static const tEncoderOps mt6816_ops = {
    .init = mt6816_init,
    .open = mt6816_open,
    .read = mt6816_read,
    .close = mt6816_close,
};

static tMt6816Dev s_int = {
    .cs_port = INTCS_GPIO_Port,
    .cs_pin = INTCS_Pin,
};
static tMt6816Dev s_ext = {
    .cs_port = EXTCS_GPIO_Port,
    .cs_pin = EXTCS_Pin,
};

static const tEncDevHandle s_int_handle = {
    .ops = &mt6816_ops,
    .dev = &s_int,
};
static const tEncDevHandle s_ext_handle = {
    .ops = &mt6816_ops,
    .dev = &s_ext,
};

const tEncDevHandle *Get_EncIntHandle(void) { return &s_int_handle; }
const tEncDevHandle *Get_EncMT6816Handle(void) { return &s_ext_handle; }

static bool mt6816_start_high(tMt6816Dev *inst) {
    inst->tx = MT6816_CMD_HIGH;
    inst->phase = MT6816_PHASE_HIGH;
    return bsp_enc_spi_start(&inst->tx, &inst->rx, 1U);
}

// DMA 完成中断：推进两帧协议并发布最近一次有效样本。
static void mt6816_callback(tBspEncSpiResult result, void *context) {
    tMt6816Dev *inst = context;
    if (!inst || inst->phase == MT6816_PHASE_IDLE)
        return;

    if (result != BSP_ENC_SPI_OK) {
        inst->phase = MT6816_PHASE_IDLE;
        inst->error_count++;
        return;
    }

    if (inst->phase == MT6816_PHASE_HIGH) {
        inst->high = (uint8_t)(inst->rx & 0xFFU);
        inst->tx = MT6816_CMD_LOW;
        inst->phase = MT6816_PHASE_LOW;
        if (!bsp_enc_spi_start(&inst->tx, &inst->rx, 1U)) {
            inst->phase = MT6816_PHASE_IDLE;
            inst->error_count++;
        }
        return;
    }

    uint8_t low = (uint8_t)(inst->rx & 0xFFU);
    uint16_t raw = (uint16_t)(((uint16_t)inst->high << 6) |
                              (((uint16_t)low & 0xFCU) >> 2));
    tEncSample sample = {
        .ts_us = bsp_time_us(),
        .angle_rad = (float)raw * (XMATH_2PI / (float)MT6816_RESOLUTION),
        .angle_kind = BSP_ANGLE_MECHANICAL,
        .referenced = true,
    };
    inst->latest = sample;
    inst->latest_valid = true;
    inst->phase = MT6816_PHASE_IDLE;
}

static bool mt6816_init(void *dev, void *cfg) {
    tMt6816Dev *inst = dev;
    if (!inst)
        return false;
    inst->initialized = false;
    inst->phase = MT6816_PHASE_IDLE;
    inst->latest_valid = false;
    inst->error_count = 0;
    return true;
}

static bool mt6816_open(void *dev) {
    tMt6816Dev *inst = dev;
    if (!inst)
        return false;

    tBspEncSpiConfig config = {
        .cs_port = inst->cs_port,
        .cs_pin = inst->cs_pin,
        .mode = BSP_ENC_SPI_MODE_3,
        .data_bits = BSP_ENC_SPI_DATA_16BIT,
        .callback = mt6816_callback,
        .context = inst,
    };
    if (!bsp_enc_spi_init(&config))
        return false;

    inst->initialized = true;
    inst->phase = MT6816_PHASE_IDLE;
    inst->latest_valid = false;
    inst->error_count = 0;
    return true;
}

static bool mt6816_read(void *dev, tEncSample *sample) {
    tMt6816Dev *inst = dev;
    if (!inst || !inst->initialized || !sample)
        return false;

    if (inst->phase == MT6816_PHASE_IDLE && !bsp_enc_spi_is_busy()) {
        if (!mt6816_start_high(inst)) {
            inst->phase = MT6816_PHASE_IDLE;
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

static bool mt6816_close(void *dev) {
    tMt6816Dev *inst = dev;
    if (!inst)
        return false;
    inst->initialized = false;
    inst->phase = MT6816_PHASE_IDLE;
    inst->latest_valid = false;
    return true;
}
