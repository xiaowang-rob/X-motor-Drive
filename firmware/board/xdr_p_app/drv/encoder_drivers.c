// ============================================================
// encoder_drivers.c — 编码器 SPI 读角引擎（板级，直连 HAL）
//
// 直接操作本板编码器 SPI 与内外两路 CS：
//   - enc_spi_set_mode：按芯片协议重配 SPI（HAL 字段修改 + Init）
//   - enc_engine_read：CS 低 → 逐段 HAL 同步收发 → CS 高
// 实例形态：文件内静态 handle（hspi / 内外 CS / 数据宽度）。
// 厂商库符号只出现在本文件。
// ============================================================
#include "encoder_drivers.h"

#include "spi.h"

#define ENC_XFER_TIMEOUT_MS 100U // SPI 读取超时时间（ms）

// ---------- 实例 handle ----------
typedef struct
{
    SPI_HandleTypeDef *hspi;   // 外设：编码器 SPI
    GPIO_TypeDef *cs_int_port; // 配置：内编 CS 端口
    uint16_t cs_int_pin;       // 配置：内编 CS 引脚
    GPIO_TypeDef *cs_ext_port; // 配置：外编 CS 端口
    uint16_t cs_ext_pin;       // 配置：外编 CS 引脚
    uint8_t data_bits;         // 运行时：当前 SPI 数据宽度
} tEncEngine;

// 静态实例
// TODO: 内/外 CS 目前配置为同一引脚，待硬件确认后修正
static tEncEngine s_engine = {
    .hspi = &hspi3,
    .cs_int_port = GPIOA,
    .cs_int_pin = GPIO_PIN_15,
    .cs_ext_port = GPIOA,
    .cs_ext_pin = GPIO_PIN_15,
    .data_bits = 16U,
};

EncEngineHandle enc_engine_get_handle(void)
{
    return (EncEngineHandle)&s_engine;
}

static void enc_cs(tEncEngine *e, eEncoderType type, bool active)
{
    if (type == EXT_ENCODER)
        HAL_GPIO_WritePin(e->cs_ext_port, e->cs_ext_pin,
                          active ? GPIO_PIN_RESET : GPIO_PIN_SET);
    else
        HAL_GPIO_WritePin(e->cs_int_port, e->cs_int_pin,
                          active ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

bool enc_spi_set_mode(EncEngineHandle h, uint8_t cpol, uint8_t cpha, uint8_t data_bits)
{
    tEncEngine *e = (tEncEngine *)h;
    if (!e || !e->hspi)
        return false;
    if (data_bits != 8U && data_bits != 16U)
        return false;

    SPI_HandleTypeDef *hspi = e->hspi;
    hspi->Init.Mode = SPI_MODE_MASTER;
    hspi->Init.Direction = SPI_DIRECTION_2LINES;
    hspi->Init.DataSize = (data_bits == 16U) ? SPI_DATASIZE_16BIT : SPI_DATASIZE_8BIT;
    hspi->Init.CLKPolarity = cpol ? SPI_POLARITY_HIGH : SPI_POLARITY_LOW;
    hspi->Init.CLKPhase = cpha ? SPI_PHASE_2EDGE : SPI_PHASE_1EDGE;
    hspi->Init.NSS = SPI_NSS_SOFT;
    hspi->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
    hspi->Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi->Init.TIMode = SPI_TIMODE_DISABLE;
    hspi->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi->Init.CRCPolynomial = 10;

    if (HAL_SPI_Init(hspi) != HAL_OK)
        return false;

    e->data_bits = data_bits;
    return true;
}

bool enc_engine_read(EncEngineHandle h, const tEncXferSeg *segs, eEncoderType type,
                     uint8_t n, uint32_t *ms)
{
    tEncEngine *e = (tEncEngine *)h;
    if (!e || !segs || n == 0U || !ms || !e->hspi)
        return false;
    if (HAL_SPI_GetState(e->hspi) != HAL_SPI_STATE_READY)
        return false;

    enc_cs(e, type, true);
    for (uint8_t i = 0U; i < n; i++)
    {
        uint16_t units = (e->data_bits == 16U) ? (segs[i].len / 2U) : segs[i].len;
        if (units == 0U ||
            HAL_SPI_TransmitReceive(e->hspi, (uint8_t *)segs[i].tx,
                                    segs[i].rx, units, ENC_XFER_TIMEOUT_MS) != HAL_OK)
        {
            enc_cs(e, type, false);
            return false;
        }
    }
    enc_cs(e, type, false);
    *ms = HAL_GetTick();
    return true;
}

void enc_engine_abort(EncEngineHandle h, eEncoderType type)
{
    tEncEngine *e = (tEncEngine *)h;
    if (!e)
        return;
    enc_cs(e, type, false);
}
