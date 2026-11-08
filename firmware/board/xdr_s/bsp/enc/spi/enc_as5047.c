// AS5047：Mode1/16bit，读命令与 NOP 各占一次 CS 事务。
// 本驱动直接基于 bsp_enc_spi 的 DMA 异步事务。

#include "enc_as5047.h"

#include "bsp_enc_spi.h"
#include "bsp_mcu.h"
#include "xmath.h"

#define AS5047_RESOLUTION 16384U

typedef enum {
    AS5047_PHASE_IDLE = 0,
    AS5047_PHASE_READ,
    AS5047_PHASE_NOP,
} tAs5047Phase;

typedef struct {
    GPIO_TypeDef *cs_port;
    uint32_t cs_pin;
    bool initialized;

    volatile tAs5047Phase phase;
    uint16_t tx;
    uint16_t rx;

    tEncSample latest;
    volatile bool latest_valid;
    volatile uint32_t error_count;
} tAs5047Dev;

static bool as5047_init(void *dev, void *cfg);
static bool as5047_open(void *dev);
static bool as5047_read(void *dev, tEncSample *sample);
static bool as5047_close(void *dev);

static const tEncoderOps as5047_ops = {
    .init = as5047_init,
    .open = as5047_open,
    .read = as5047_read,
    .close = as5047_close,
};

static tAs5047Dev s_ext = {
    .cs_port = EXTCS_GPIO_Port,
    .cs_pin = EXTCS_Pin,
};

static const tEncDevHandle s_ext_handle = {
    .ops = &as5047_ops,
    .dev = &s_ext,
};

const tEncDevHandle *Get_EncAS5047Handle(void) { return &s_ext_handle; }

static bool as5047_parity_odd(uint16_t value) {
    value ^= (uint16_t)(value >> 8);
    value ^= (uint16_t)(value >> 4);
    value ^= (uint16_t)(value >> 2);
    value ^= (uint16_t)(value >> 1);
    return (value & 1U) != 0U;
}

static void as5047_callback(tBspEncSpiResult result, void *context) {
    tAs5047Dev *inst = context;
    if (!inst || inst->phase == AS5047_PHASE_IDLE)
        return;

    if (result != BSP_ENC_SPI_OK) {
        inst->phase = AS5047_PHASE_IDLE;
        inst->error_count++;
        return;
    }

    if (inst->phase == AS5047_PHASE_READ) {
        inst->tx = 0x0000U; // NOP，接收上一帧命令的响应
        inst->phase = AS5047_PHASE_NOP;
        if (!bsp_enc_spi_start(&inst->tx, &inst->rx, 1U)) {
            inst->phase = AS5047_PHASE_IDLE;
            inst->error_count++;
        }
        return;
    }

    uint16_t frame = inst->rx;
    // bit14 为错误标志；整个响应帧采用偶校验。
    if ((frame & 0x4000U) != 0U || as5047_parity_odd(frame)) {
        inst->phase = AS5047_PHASE_IDLE;
        inst->error_count++;
        return;
    }

    uint16_t raw = (uint16_t)(frame & 0x3FFFU);
    tEncSample sample = {
        .ts_us = bsp_time_us(),
        .angle_rad = (float)raw * (XMATH_2PI / (float)AS5047_RESOLUTION),
        .angle_kind = BSP_ANGLE_MECHANICAL,
        .referenced = true,
    };
    inst->latest = sample;
    inst->latest_valid = true;
    inst->phase = AS5047_PHASE_IDLE;
}

static bool as5047_init(void *dev, void *cfg) {
    tAs5047Dev *inst = dev;
    if (!inst)
        return false;
    inst->initialized = false;
    inst->phase = AS5047_PHASE_IDLE;
    inst->latest_valid = false;
    inst->error_count = 0;
    return true;
}

static bool as5047_open(void *dev) {
    tAs5047Dev *inst = dev;
    if (!inst)
        return false;

    tBspEncSpiConfig config = {
        .cs_port = inst->cs_port,
        .cs_pin = inst->cs_pin,
        .mode = BSP_ENC_SPI_MODE_1,
        .data_bits = BSP_ENC_SPI_DATA_16BIT,
        .callback = as5047_callback,
        .context = inst,
    };
    if (!bsp_enc_spi_init(&config))
        return false;

    inst->initialized = true;
    inst->phase = AS5047_PHASE_IDLE;
    inst->latest_valid = false;
    inst->error_count = 0;
    return true;
}

static bool as5047_read(void *dev, tEncSample *sample) {
    tAs5047Dev *inst = dev;
    if (!inst || !inst->initialized || !sample)
        return false;

    if (inst->phase == AS5047_PHASE_IDLE && !bsp_enc_spi_is_busy()) {
        inst->tx = 0xFFFFU; // 读 0x3FFF，含偶校验位
        inst->phase = AS5047_PHASE_READ;
        if (!bsp_enc_spi_start(&inst->tx, &inst->rx, 1U)) {
            inst->phase = AS5047_PHASE_IDLE;
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

static bool as5047_close(void *dev) {
    tAs5047Dev *inst = dev;
    if (!inst)
        return false;
    inst->initialized = false;
    inst->phase = AS5047_PHASE_IDLE;
    inst->latest_valid = false;
    return true;
}
