// ============================================================
// enc_spi_engine.c — 编码器 SPI 读角时序（v2 直连版）
//
// 直接操作本板编码器 SPI（ENCODER_SPI_CH/hspi3）与 CS（GPIO）：
//   - enc_spi_set_mode：按芯片协议重配 SPI3（HAL 字段修改 + Init）
//   - enc_engine_read：CS 低 → 逐段 HAL 同步收发 → CS 高
// 厂商库符号只出现在本文件与 platform.h。
// ============================================================

#include "enc_spi_engine.h"

#include "platform.h"

#define ENC_XFER_TIMEOUT_MS 100U

static uint8_t s_data_bits = 16U; // 当前 SPI 数据宽度（set_mode 维护）

static void enc_cs(bool active)
{
    HAL_GPIO_WritePin(ENCODER_INT_CS_GPIOx, ENCODER_INT_CS_GPIOx_PIN,
                      active ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

bool enc_spi_set_mode(uint8_t cpol, uint8_t cpha, uint8_t data_bits)
{
    if (data_bits != 8U && data_bits != 16U)
        return false;

    SPI_HandleTypeDef *h = &ENCODER_SPI_CH;
    h->Init.Mode = SPI_MODE_MASTER;
    h->Init.Direction = SPI_DIRECTION_2LINES;
    h->Init.DataSize = (data_bits == 16U) ? SPI_DATASIZE_16BIT : SPI_DATASIZE_8BIT;
    h->Init.CLKPolarity = cpol ? SPI_POLARITY_HIGH : SPI_POLARITY_LOW;
    h->Init.CLKPhase = cpha ? SPI_PHASE_2EDGE : SPI_PHASE_1EDGE;
    h->Init.NSS = SPI_NSS_SOFT;
    h->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
    h->Init.FirstBit = SPI_FIRSTBIT_MSB;
    h->Init.TIMode = SPI_TIMODE_DISABLE;
    h->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    h->Init.CRCPolynomial = 10;

    if (HAL_SPI_Init(h) != HAL_OK)
        return false;

    s_data_bits = data_bits;
    return true;
}

bool enc_engine_read(const tEncXferSeg *segs, uint8_t n)
{
    if (!segs || n == 0U)
        return false;
    if (HAL_SPI_GetState(&ENCODER_SPI_CH) != HAL_SPI_STATE_READY)
        return false;

    enc_cs(true);
    for (uint8_t i = 0U; i < n; i++)
    {
        uint16_t units = (s_data_bits == 16U) ? (segs[i].len / 2U) : segs[i].len;
        if (units == 0U ||
            HAL_SPI_TransmitReceive(&ENCODER_SPI_CH, (uint8_t *)segs[i].tx,
                                    segs[i].rx, units, ENC_XFER_TIMEOUT_MS) != HAL_OK)
        {
            enc_cs(false);
            return false;
        }
    }
    enc_cs(false);
    return true;
}

void enc_engine_abort(void)
{
    enc_cs(false);
}

uint32_t enc_tick_ms(void)
{
    return platform_get_ms();
}
