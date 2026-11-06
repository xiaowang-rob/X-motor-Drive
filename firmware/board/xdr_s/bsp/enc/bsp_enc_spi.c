
#include "bsp_enc_spi.h"
#include "dma.h"
#include "main.h"

/* ======================== 硬件配置 ======================== */

#define ENC_SPI SPI1

#define ENC_SPI_RX_DMA DMA1
#define ENC_SPI_RX_CHANNEL LL_DMA_CHANNEL_5

#define ENC_SPI_TX_DMA DMA1
#define ENC_SPI_TX_CHANNEL LL_DMA_CHANNEL_6

/*
 * RX DMA 完成后，等待 SPI BSY 清零。
 * 这是有限循环，不能在中断中无限等待。
 * 实际等待时间需要结合 SPI 时钟和 MCU 主频验证。
 */
#define ENC_SPI_BSY_TIMEOUT 10000U

/* ======================== 内部状态 ======================== */

typedef enum {
    ENC_SPI_STATE_UNINIT = 0,
    ENC_SPI_STATE_IDLE,
    ENC_SPI_STATE_BUSY,
} tEncSpiState;

static struct {
    tEncSpiState state;

    GPIO_TypeDef *cs_port;
    uint32_t cs_pin;

    tBspEncSpiMode mode;
    tBspEncSpiDataBits data_bits;

    tBspEncSpiCallback callback;
    void *context;
} s_enc_spi;

static inline void enc_spi_cs_low(void) {
    LL_GPIO_ResetOutputPin(s_enc_spi.cs_port, s_enc_spi.cs_pin);
}

static inline void enc_spi_cs_high(void) {
    LL_GPIO_SetOutputPin(s_enc_spi.cs_port, s_enc_spi.cs_pin);
}

/* ======================== DMA 配置 ======================== */

static void enc_spi_dma_configure_width(tBspEncSpiDataBits data_bits) {
    uint32_t periph_width;
    uint32_t memory_width;

    if (data_bits == BSP_ENC_SPI_DATA_8BIT) {
        periph_width = LL_DMA_PDATAALIGN_BYTE;
        memory_width = LL_DMA_MDATAALIGN_BYTE;
    } else {
        periph_width = LL_DMA_PDATAALIGN_HALFWORD;
        memory_width = LL_DMA_MDATAALIGN_HALFWORD;
    }

    LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_5, periph_width);
    LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_5, memory_width);

    LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_6, periph_width);
    LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_6, memory_width);
}

static void enc_spi_dma_stop(void) {
    LL_SPI_DisableDMAReq_RX(ENC_SPI);
    LL_SPI_DisableDMAReq_TX(ENC_SPI);

    LL_DMA_DisableChannel(ENC_SPI_RX_DMA, ENC_SPI_RX_CHANNEL);
    LL_DMA_DisableChannel(ENC_SPI_TX_DMA, ENC_SPI_TX_CHANNEL);
}

static void enc_spi_dma_clear_flags(void) {
    LL_DMA_ClearFlag_GI5(DMA1);
    LL_DMA_ClearFlag_GI6(DMA1);
}

/* ======================== 错误标志处理 ======================== */

static void enc_spi_clear_error_flags(void) {
    bool was_enabled = LL_SPI_IsEnabled(ENC_SPI);

    if (LL_SPI_IsActiveFlag_OVR(ENC_SPI)) {
        LL_SPI_ClearFlag_OVR(ENC_SPI);
    }

    if (LL_SPI_IsActiveFlag_MODF(ENC_SPI)) {
        LL_SPI_ClearFlag_MODF(ENC_SPI);
    }

    if (LL_SPI_IsActiveFlag_CRCERR(ENC_SPI)) {
        LL_SPI_ClearFlag_CRCERR(ENC_SPI);
    }

    if (LL_SPI_IsActiveFlag_FRE(ENC_SPI)) {
        LL_SPI_ClearFlag_FRE(ENC_SPI);
    }

    if (was_enabled && !LL_SPI_IsEnabled(ENC_SPI)) {
        LL_SPI_Enable(ENC_SPI);
    }
}

/* ======================== 传输收尾 ======================== */

static void enc_spi_finish(tBspEncSpiResult result) {
    tBspEncSpiCallback callback;
    void *context;

    if (s_enc_spi.state != ENC_SPI_STATE_BUSY) {
        return;
    }

    enc_spi_dma_stop();

    /*
     * 正常完成时，RX DMA TC 表示最后一个数据已进入接收缓冲区。
     * 仍需确认 SPI BSY 清零，避免 CS 提前释放。
     */
    if (result == BSP_ENC_SPI_OK) {
        uint32_t timeout = ENC_SPI_BSY_TIMEOUT;

        while (LL_SPI_IsActiveFlag_BSY(ENC_SPI) && timeout > 0U) {
            timeout--;
        }

        if (LL_SPI_IsActiveFlag_BSY(ENC_SPI)) {
            result = BSP_ENC_SPI_ERR_SPI;
        }
    }

    enc_spi_clear_error_flags();
    enc_spi_dma_clear_flags();

    enc_spi_cs_high();

    callback = s_enc_spi.callback;
    context = s_enc_spi.context;

    s_enc_spi.state = ENC_SPI_STATE_IDLE;

    if (callback != NULL) {
        callback(result, context);
    }
}

/* ======================== 对外接口 ======================== */

bool bsp_enc_spi_init(const tBspEncSpiConfig *config) {
    LL_SPI_InitTypeDef SPI_InitStruct = {0};
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 参数检查 */
    if (config == NULL || config->cs_port == NULL || config->cs_pin == 0U) {
        return false;
    }

    if (config->mode > BSP_ENC_SPI_MODE_3) {
        return false;
    }

    if (config->data_bits != BSP_ENC_SPI_DATA_8BIT &&
        config->data_bits != BSP_ENC_SPI_DATA_16BIT) {
        return false;
    }

    if (s_enc_spi.state == ENC_SPI_STATE_BUSY) {
        return false;
    }

    /* 保存配置 */
    s_enc_spi.state = ENC_SPI_STATE_UNINIT;

    s_enc_spi.cs_port = config->cs_port;
    s_enc_spi.cs_pin = config->cs_pin;
    s_enc_spi.mode = config->mode;
    s_enc_spi.data_bits = config->data_bits;
    s_enc_spi.callback = config->callback;
    s_enc_spi.context = config->context;

    /* ==================== 时钟使能 ==================== */

    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);

    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);

    /* ==================== CS GPIO 初始化 ==================== */

    enc_spi_cs_high();

    /* ==================== SPI GPIO 初始化 ==================== */

    /*
     * PA5 -> SPI1_SCK
     * PA6 -> SPI1_MISO
     * PA7 -> SPI1_MOSI
     */

    GPIO_InitStruct.Pin = LL_GPIO_PIN_5;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStruct.Alternate = LL_GPIO_AF_5;
    LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LL_GPIO_PIN_6;
    LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LL_GPIO_PIN_7;
    LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* ==================== DMA 初始化 ==================== */

    /* SPI1 RX: DMA1 Channel 5 */

    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_5, LL_DMAMUX_REQ_SPI1_RX);

    LL_DMA_SetDataTransferDirection(
        DMA1, LL_DMA_CHANNEL_5, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

    LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_5, LL_DMA_PRIORITY_LOW);

    LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_5, LL_DMA_MODE_NORMAL);

    LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_5, LL_DMA_PERIPH_NOINCREMENT);

    LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_5, LL_DMA_MEMORY_INCREMENT);

    /* SPI1 TX: DMA1 Channel 6 */

    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_6, LL_DMAMUX_REQ_SPI1_TX);

    LL_DMA_SetDataTransferDirection(
        DMA1, LL_DMA_CHANNEL_6, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);

    LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_6, LL_DMA_PRIORITY_LOW);

    LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_6, LL_DMA_MODE_NORMAL);

    LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_6, LL_DMA_PERIPH_NOINCREMENT);

    LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_6, LL_DMA_MEMORY_INCREMENT);

    /* 根据 SPI 数据位宽配置 DMA */
    enc_spi_dma_configure_width(config->data_bits);

    /* ==================== SPI 参数初始化 ==================== */

    SPI_InitStruct.TransferDirection = LL_SPI_FULL_DUPLEX;
    SPI_InitStruct.Mode = LL_SPI_MODE_MASTER;

    SPI_InitStruct.DataWidth = (config->data_bits == BSP_ENC_SPI_DATA_8BIT)
                                   ? LL_SPI_DATAWIDTH_8BIT
                                   : LL_SPI_DATAWIDTH_16BIT;

    switch (config->mode) {

    case BSP_ENC_SPI_MODE_0:
        SPI_InitStruct.ClockPolarity = LL_SPI_POLARITY_LOW;
        SPI_InitStruct.ClockPhase = LL_SPI_PHASE_1EDGE;
        break;

    case BSP_ENC_SPI_MODE_1:
        SPI_InitStruct.ClockPolarity = LL_SPI_POLARITY_LOW;
        SPI_InitStruct.ClockPhase = LL_SPI_PHASE_2EDGE;
        break;

    case BSP_ENC_SPI_MODE_2:
        SPI_InitStruct.ClockPolarity = LL_SPI_POLARITY_HIGH;
        SPI_InitStruct.ClockPhase = LL_SPI_PHASE_1EDGE;
        break;

    case BSP_ENC_SPI_MODE_3:
        SPI_InitStruct.ClockPolarity = LL_SPI_POLARITY_HIGH;
        SPI_InitStruct.ClockPhase = LL_SPI_PHASE_2EDGE;
        break;

    default:
        return false;
    }

    SPI_InitStruct.NSS = LL_SPI_NSS_SOFT;
    SPI_InitStruct.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV32;
    SPI_InitStruct.BitOrder = LL_SPI_MSB_FIRST;
    SPI_InitStruct.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;
    SPI_InitStruct.CRCPoly = 7U;

    /* SPI 必须处于禁用状态 */
    LL_SPI_Disable(SPI1);

    if (LL_SPI_Init(SPI1, &SPI_InitStruct) != SUCCESS) {
        return false;
    }

    LL_SPI_SetStandard(SPI1, LL_SPI_PROTOCOL_MOTOROLA);
    LL_SPI_DisableNSSPulseMgt(SPI1);

    /* RX FIFO 阈值 */
    if (config->data_bits == BSP_ENC_SPI_DATA_8BIT) {
        LL_SPI_SetRxFIFOThreshold(SPI1, LL_SPI_RX_FIFO_TH_QUARTER);
    } else {
        LL_SPI_SetRxFIFOThreshold(SPI1, LL_SPI_RX_FIFO_TH_HALF);
    }

    /* ==================== NVIC 初始化 ==================== */

    NVIC_SetPriority(SPI1_IRQn,
                     NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 1, 1));

    NVIC_EnableIRQ(SPI1_IRQn);

    /* ==================== DMA 中断配置 ==================== */

    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_5);
    LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_5);

    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_6);
    LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_6);

    /* SPI 错误中断 */
    LL_SPI_EnableIT_ERR(SPI1);

    /* 清理状态 */
    enc_spi_dma_stop();
    enc_spi_dma_clear_flags();
    enc_spi_clear_error_flags();

    /* CS 默认不选中 */
    enc_spi_cs_high();

    /* 启动 SPI */
    LL_SPI_Enable(SPI1);

    s_enc_spi.state = ENC_SPI_STATE_IDLE;

    return true;
}

bool bsp_enc_spi_start(const void *tx_buf, void *rx_buf, uint16_t frame_count) {
    if (s_enc_spi.state == ENC_SPI_STATE_UNINIT) {
        return false;
    }

    if (tx_buf == NULL || rx_buf == NULL || frame_count == 0U) {
        return false;
    }

    if (s_enc_spi.state == ENC_SPI_STATE_BUSY) {
        return false;
    }

    /* 确保 SPI 已经空闲 */
    if (LL_SPI_IsActiveFlag_BSY(ENC_SPI)) {
        return false;
    }

    enc_spi_dma_stop();
    enc_spi_dma_clear_flags();
    enc_spi_clear_error_flags();

    /* RX DMA：SPI -> 内存 */
    LL_DMA_ConfigAddresses(DMA1,
                           LL_DMA_CHANNEL_5,
                           (uint32_t)&ENC_SPI->DR,
                           (uint32_t)rx_buf,
                           LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_5, frame_count);

    /* TX DMA：内存 -> SPI */
    LL_DMA_ConfigAddresses(DMA1,
                           LL_DMA_CHANNEL_6,
                           (uint32_t)tx_buf,
                           (uint32_t)&ENC_SPI->DR,
                           LL_DMA_DIRECTION_MEMORY_TO_PERIPH);

    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_6, frame_count);

    s_enc_spi.state = ENC_SPI_STATE_BUSY;

    /* CS 拉低，开始一次完整事务 */
    enc_spi_cs_low();

    /*
     * 先开启 RX，再开启 TX，避免首个接收数据丢失。
     */
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_5);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_6);

    LL_SPI_EnableDMAReq_RX(ENC_SPI);
    LL_SPI_EnableDMAReq_TX(ENC_SPI);

    return true;
}

bool bsp_enc_spi_is_busy(void) {
    return (s_enc_spi.state == ENC_SPI_STATE_BUSY);
}

/* ======================== DMA 中断入口 ======================== */

void bsp_enc_spi_dma_rx_irq(void) {
    if (LL_DMA_IsActiveFlag_TE5(DMA1)) {
        LL_DMA_ClearFlag_GI5(DMA1);
        enc_spi_finish(BSP_ENC_SPI_ERR_DMA);
        return;
    }

    if (LL_DMA_IsActiveFlag_TC5(DMA1)) {
        LL_DMA_ClearFlag_TC5(DMA1);
        enc_spi_finish(BSP_ENC_SPI_OK);
    }
}

void bsp_enc_spi_dma_tx_irq(void) {
    if (LL_DMA_IsActiveFlag_TE6(DMA1)) {
        LL_DMA_ClearFlag_GI6(DMA1);
        enc_spi_finish(BSP_ENC_SPI_ERR_DMA);
        return;
    }

    if (LL_DMA_IsActiveFlag_TC6(DMA1)) {
        LL_DMA_ClearFlag_TC6(DMA1);
    }
}

/* ======================== SPI 中断入口 ======================== */

void bsp_enc_spi_spi_irq(void) {
    if (LL_SPI_IsActiveFlag_OVR(ENC_SPI) || LL_SPI_IsActiveFlag_MODF(ENC_SPI) ||
        LL_SPI_IsActiveFlag_CRCERR(ENC_SPI) ||
        LL_SPI_IsActiveFlag_FRE(ENC_SPI)) {

        enc_spi_clear_error_flags();
        enc_spi_finish(BSP_ENC_SPI_ERR_SPI);
    }
}
