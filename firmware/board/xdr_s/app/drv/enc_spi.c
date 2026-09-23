// ============================================================
// enc_spi.c — 编码器 SPI 读角引擎（板级，直连 HAL）
//
// 直接操作本板编码器 SPI 与两路 CS：
//   - enc_spi_ensure_mode：幂等重配 SPI（HAL 字段修改 + Init）
//   - enc_spi_transfer   ：CS 低 → 逐段 HAL 同步收发 → CS 高
// CS 由调用方（芯片驱动实例）以 tEncCs 传入，本文件只做电平动作。
// ============================================================
#include "enc_spi.h"

#include "bsp_time.h"
#include "spi.h"

#define ENC_XFER_TIMEOUT_MS 100U // SPI 读取超时时间（ms）

// CS 描述（片选：端口 + 引脚）
typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} tEncCs;

// ---------- SPI 单例 —— 外设 + 当前模式 ----------
typedef struct
{
    SPI_HandleTypeDef *hspi; // 外设：编码器 SPI
    tEncCs cs[2];
    bool mode_valid; // 当前模式是否已知
    uint8_t cpol;
    uint8_t cpha;
    uint8_t data_bits;
} tEncSpi;

static tEncSpi s_spi = {
    .hspi = &hspi1,
    .mode_valid = false,
    .cs[ENC_INT] = {.port = GPIOB, .pin = GPIO_PIN_6},
    .cs[ENC_EXT] = {.port = GPIOB, .pin = GPIO_PIN_7},
};

static void enc_cs(const tEncCs *cs, bool active)
{
    if (!cs)
        return;
    HAL_GPIO_WritePin(cs->port, cs->pin, active ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

bool enc_spi_ensure_mode(uint8_t cpol, uint8_t cpha, uint8_t data_bits)
{
    if (!s_spi.hspi)
        return false;
    if (data_bits != 8U && data_bits != 16U)
        return false;

    // 已是目标模式：不重配（HAL_SPI_Init 有开销，且会打断总线）
    if (s_spi.mode_valid && s_spi.cpol == cpol && s_spi.cpha == cpha &&
        s_spi.data_bits == data_bits)
        return true;

    SPI_HandleTypeDef *hspi = s_spi.hspi;
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
    {
        s_spi.mode_valid = false;
        return false;
    }

    s_spi.cpol = cpol;
    s_spi.cpha = cpha;
    s_spi.data_bits = data_bits;
    s_spi.mode_valid = true;
    return true;
}

bool enc_spi_transfer(const tEncXferSeg *segs, eENCtype type, uint8_t n, uint32_t *ts_ms)
{
    if (!segs || n == 0U || !ts_ms || !s_spi.hspi)
        return false;
    if (HAL_SPI_GetState(s_spi.hspi) != HAL_SPI_STATE_READY)
        return false;

    enc_cs(&s_spi.cs[type], true);
    for (uint8_t i = 0U; i < n; i++)
    {
        uint16_t units = (s_spi.data_bits == 16U) ? (uint16_t)(segs[i].len / 2U) : segs[i].len;
        if (units == 0U ||
            HAL_SPI_TransmitReceive(s_spi.hspi, (uint8_t *)segs[i].tx,
                                    segs[i].rx, units, ENC_XFER_TIMEOUT_MS) != HAL_OK)
        {
            enc_cs(&s_spi.cs[type], false);
            return false;
        }
    }
    enc_cs(&s_spi.cs[type], false);

    *ts_ms = bsp_time_ms(); // 时间戳统一走 bsp
    return true;
}

void enc_spi_abort(eENCtype type)
{
    enc_cs(&s_spi.cs[type], false);
}
