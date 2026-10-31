// SPI 编码器专用驱动；芯片协议保持不变。

#include "bsp_enc_spi.h"
#include "bsp_mcu.h"
#include "gpio.h"
#include "spi.h"
#include "xmath.h"

struct SpiEncDev {
    struct {
        GPIO_TypeDef *port;
        uint32_t pin;
    } cs;
    eSpiEncList chip;
    const tSpiEncChipOps *chip_ops;
    float rad_per_lsb;
    bool inited;
};

static const tSpiEncChipOps *enc_chip_ops(eSpiEncList chip);
static bool spi_enc_open(void *handle);
static bool enc_spi_close(void *handle);
static bool spi_enc_read(void *handle, tEncSample *sample);
static void spi_enc_abort(void *handle);

const tEncoderOps spi_enc_ops = {
    .open = spi_enc_open,
    .close = enc_spi_close,
    .read = spi_enc_read,
    .abort = spi_enc_abort,
};

static tSpiEncDev spi_enc_int = {
    .cs = {.port = INTCS_GPIO_Port, .pin = INTCS_Pin},
};
static tSpiEncDev spi_enc_ext = {
    .cs = {.port = EXTCS_GPIO_Port, .pin = EXTCS_Pin},
};

const tEncDevHandle INT_ENCHandle = {
    .dev = &spi_enc_int,
    .ops = &spi_enc_ops,
};

const tEncDevHandle SPI_ENCHandle = {
    .dev = &spi_enc_ext,
    .ops = &spi_enc_ops,
};

const tEncDevHandle *Get_INT_ENCHandle(void) {
    spi_enc_int.chip = ENC_MT6816;
    spi_enc_int.chip_ops = enc_chip_ops(ENC_MT6816);
    return &INT_ENCHandle;
}
const tEncDevHandle *Get_SPI_ENCHandle(eSpiEncList chip) {
    spi_enc_ext.chip = chip;
    spi_enc_ext.chip_ops = enc_chip_ops(chip);
    return &SPI_ENCHandle;
}

// 锁只保护同步事务，不在整个传输期间关中断；中断重入直接失败。
static tSpiEncDev *s_owner;
static bool enc_spi_prepare(const tSpiEncChipOps *ops);
static void enc_spi_reset(void);
static void enc_cs(tSpiEncDev *dev, bool active);

static bool enc_valid_dev(const tSpiEncDev *dev) {
    return dev == &spi_enc_int || dev == &spi_enc_ext;
}

static bool enc_spi_lock(tSpiEncDev *dev) {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    bool acquired = s_owner == NULL;
    if (acquired)
        s_owner = dev;
    __set_PRIMASK(primask);
    return acquired;
}

static void enc_spi_unlock(void) {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    s_owner = NULL;
    __set_PRIMASK(primask);
}

static void Mode_Switch_SPI(void) {
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);
    const uint32_t pins[] = {LL_GPIO_PIN_5, LL_GPIO_PIN_6, LL_GPIO_PIN_7};
    for (unsigned i = 0; i < 3; ++i) {
        LL_GPIO_SetAFPin_0_7(GPIOA, pins[i], LL_GPIO_AF_5);
        LL_GPIO_SetPinOutputType(GPIOA, pins[i], LL_GPIO_OUTPUT_PUSHPULL);
        LL_GPIO_SetPinSpeed(GPIOA, pins[i], LL_GPIO_SPEED_FREQ_LOW);
        LL_GPIO_SetPinMode(GPIOA, pins[i], LL_GPIO_MODE_ALTERNATE);
    }
}

static const tSpiEncChipOps *enc_chip_ops(eSpiEncList chip) {
    switch (chip) {
    case ENC_MT6816:
        return &spi_mt6816_ops;
    case ENC_MT6835:
        return &spi_mt6835_ops;
    case ENC_AS5047:
        return &spi_as5047_ops;
    default:
        return NULL;
    }
}

static bool spi_enc_open(void *handle) {
    tSpiEncDev *dev = (tSpiEncDev *)handle;
    if (!enc_valid_dev(dev) || !enc_spi_lock(dev))
        return false;

    dev->inited = false;
    dev->chip_ops = NULL;

    bool ok = enc_spi_prepare(dev->chip_ops);
    if (ok) {

        dev->inited = true;
        dev->rad_per_lsb = XMATH_2PI / (float)dev->chip_ops->resolution;
    }
    enc_spi_unlock();
    return ok;
}
static bool enc_spi_close(void *handle) {
    tSpiEncDev *dev = handle;
    if (!enc_valid_dev(dev) || !enc_spi_lock(dev))
        return false;
    enc_spi_reset();
    dev->inited = false;
    dev->chip_ops = NULL;
    enc_spi_unlock();
    return true;
}

static bool spi_enc_read(void *handle, tEncSample *sample) {
    tSpiEncDev *dev = handle;
    if (!enc_valid_dev(dev) || !sample || !enc_spi_lock(dev))
        return false;

    bool ok = dev->inited && dev->chip_ops != NULL &&
              enc_spi_prepare(dev->chip_ops) &&
              dev->chip_ops->read(dev, &sample->raw);
    if (ok) {

        sample->angle_rad = sample->raw * dev->rad_per_lsb;
        sample->ts_ms = bsp_time_ms();
        sample->angle_kind = BSP_ANGLE_MECHANICAL;
    }

    enc_spi_unlock();
    return ok;
}

static void spi_enc_abort(void *handle) {
    tSpiEncDev *dev = handle;
    if (!enc_valid_dev(dev) || !enc_spi_lock(dev))
        return;
    enc_spi_reset();
    enc_spi_unlock();
}

static void enc_cs(tSpiEncDev *dev, bool active) {
    if (active)
        LL_GPIO_ResetOutputPin(dev->cs.port, dev->cs.pin);
    else
        LL_GPIO_SetOutputPin(dev->cs.port, dev->cs.pin);
}

static void enc_spi_reset(void) {
    // SPI1 专用于这两路编码器；错误恢复时停止时钟并释放全部 CS。
    LL_SPI_Disable(SPI1);
    enc_cs(&spi_enc_int, false);
    enc_cs(&spi_enc_ext, false);
    // 按 8bit 阈值清空，避免 16bit 模式残留单字节时 RXNE 不置位。
    LL_SPI_SetRxFIFOThreshold(SPI1, LL_SPI_RX_FIFO_TH_QUARTER);
    // SPI FIFO 最多 4 字节，使用有界清空，避免异常状态下死循环。
    for (uint8_t i = 0U; i < 4U && LL_SPI_IsActiveFlag_RXNE(SPI1); ++i)
        (void)LL_SPI_ReceiveData8(SPI1);
    LL_SPI_ClearFlag_OVR(SPI1);
    if (LL_SPI_IsActiveFlag_MODF(SPI1))
        LL_SPI_ClearFlag_MODF(SPI1);
    if (LL_SPI_IsActiveFlag_CRCERR(SPI1))
        LL_SPI_ClearFlag_CRCERR(SPI1);
}

static bool enc_spi_prepare(const tSpiEncChipOps *ops) {
    if (LL_SPI_IsActiveFlag_BSY(SPI1)) {
        enc_spi_reset();
        return false;
    }
    enc_spi_reset();
    LL_SPI_SetMode(SPI1, LL_SPI_MODE_MASTER);
    LL_SPI_SetTransferDirection(SPI1, LL_SPI_FULL_DUPLEX);
    LL_SPI_SetNSSMode(SPI1, LL_SPI_NSS_SOFT);
    LL_SPI_SetBaudRatePrescaler(SPI1, LL_SPI_BAUDRATEPRESCALER_DIV32);
    LL_SPI_SetTransferBitOrder(SPI1, LL_SPI_MSB_FIRST);
    LL_SPI_SetStandard(SPI1, LL_SPI_PROTOCOL_MOTOROLA);
    LL_SPI_DisableCRC(SPI1);
    LL_SPI_DisableNSSPulseMgt(SPI1);
    LL_SPI_SetClockPolarity(
        SPI1, ops->cpol ? LL_SPI_POLARITY_HIGH : LL_SPI_POLARITY_LOW);
    LL_SPI_SetClockPhase(SPI1,
                         ops->cpha ? LL_SPI_PHASE_2EDGE : LL_SPI_PHASE_1EDGE);
    LL_SPI_SetDataWidth(SPI1,
                        ops->data_bits == 16U ? LL_SPI_DATAWIDTH_16BIT
                                              : LL_SPI_DATAWIDTH_8BIT);
    LL_SPI_SetRxFIFOThreshold(SPI1,
                              ops->data_bits == 16U
                                  ? LL_SPI_RX_FIFO_TH_HALF
                                  : LL_SPI_RX_FIFO_TH_QUARTER);
    LL_SPI_Enable(SPI1);
    return true;
}

// 同时使用毫秒期限和轮询次数上限，SysTick 被屏蔽时也不会无限等待。
#define ENC_TIMEOUT_MS 2U
#define ENC_POLL_LIMIT 100000U

static bool enc_spi_wait(uint32_t (*flag)(const SPI_TypeDef *), bool set) {
    uint32_t start = bsp_time_ms();
    for (uint32_t i = 0U; i < ENC_POLL_LIMIT; ++i) {
        if (LL_SPI_IsActiveFlag_OVR(SPI1) || LL_SPI_IsActiveFlag_MODF(SPI1) ||
            LL_SPI_IsActiveFlag_FRE(SPI1))
            return false;
        if ((flag(SPI1) != 0U) == set)
            return true;
        if ((uint32_t)(bsp_time_ms() - start) >= ENC_TIMEOUT_MS)
            return false;
    }
    return false;
}

static void enc_cs_gap(void) {
    // 保证相邻命令帧间存在 CS 高电平间隔，覆盖 AS5047 的最小间隔。
    // Cortex-M4 每个 NOP 至少一周期；当前 170MHz 下至少约 0.75us。
    for (volatile uint32_t i = 0U; i < 128U; ++i)
        __NOP();
}

bool enc_spi_transfer(tSpiEncDev *dev, const uint8_t *tx, uint8_t *rx,
                      uint16_t len) {
    if (!enc_valid_dev(dev) || s_owner != dev || !dev->inited ||
        !dev->chip_ops || !tx || !rx || len == 0U)
        return false;
    bool wide = dev->chip_ops->data_bits == 16U;
    if (wide && (len & 1U) != 0U)
        return false;

    enc_cs_gap();
    enc_cs(dev, true);
    enc_cs_gap();
    for (uint16_t i = 0U; i < len; i = (uint16_t)(i + (wide ? 2U : 1U))) {
        if (!enc_spi_wait(LL_SPI_IsActiveFlag_TXE, true))
            goto fail;
        if (wide)
            LL_SPI_TransmitData16(
                SPI1, (uint16_t)(tx[i] | ((uint16_t)tx[i + 1U] << 8)));
        else
            LL_SPI_TransmitData8(SPI1, tx[i]);
        if (!enc_spi_wait(LL_SPI_IsActiveFlag_RXNE, true))
            goto fail;
        if (wide) {
            uint16_t word = LL_SPI_ReceiveData16(SPI1);
            rx[i] = (uint8_t)word;
            rx[i + 1U] = (uint8_t)(word >> 8);
        } else
            rx[i] = LL_SPI_ReceiveData8(SPI1);
    }
    if (!enc_spi_wait(LL_SPI_IsActiveFlag_TXE, true) ||
        !enc_spi_wait(LL_SPI_IsActiveFlag_BSY, false))
        goto fail;
    enc_cs_gap();
    enc_cs(dev, false);
    return true;

fail:
    enc_spi_reset();
    return false;
}
