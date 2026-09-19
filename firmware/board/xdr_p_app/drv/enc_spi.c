// ============================================================
// enc_spi.c — 编码器 SPI 读角引擎（板级，直连 HAL）
//
// 直接操作本板编码器 SPI 与内外两路 CS：
//   - enc_spi_ensure_mode：幂等重配 SPI（HAL 字段修改 + Init）
//   - enc_spi_transfer   ：CS 低 → 逐段 HAL 同步收发 → CS 高
// 实例形态：文件内静态 handle（hspi / 内外 CS / 当前模式）。
// 厂商库符号只出现在本文件。
//
// TODO: 内/外 CS 目前配置为同一引脚，待硬件确认后修正。
// ============================================================
#include "enc_spi.h"

#include "bsp_time.h"
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

    // 运行时：当前 SPI 模式（供 ensure_mode 判等）
    bool mode_valid;
    uint8_t cpol;
    uint8_t cpha;
    uint8_t data_bits;
} tEncSpi;

static tEncSpi s_spi = {
    .hspi = &hspi3,
    .cs_int_port = GPIOA,
    .cs_int_pin = GPIO_PIN_15,
    .cs_ext_port = GPIOA,
    .cs_ext_pin = GPIO_PIN_15,
    .mode_valid = false,
};

static void enc_cs(eEncoderType type, bool active)
{
    if (type == EXT_ENCODER)
        HAL_GPIO_WritePin(s_spi.cs_ext_port, s_spi.cs_ext_pin,
                          active ? GPIO_PIN_RESET : GPIO_PIN_SET);
    else
        HAL_GPIO_WritePin(s_spi.cs_int_port, s_spi.cs_int_pin,
                          active ? GPIO_PIN_RESET : GPIO_PIN_SET);
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

bool enc_spi_transfer(const tEncXferSeg *segs, eEncoderType type, uint8_t n, uint32_t *ts_ms)
{
    if (!segs || n == 0U || !ts_ms || !s_spi.hspi)
        return false;
    if (HAL_SPI_GetState(s_spi.hspi) != HAL_SPI_STATE_READY)
        return false;

    enc_cs(type, true);
    for (uint8_t i = 0U; i < n; i++)
    {
        uint16_t units = (s_spi.data_bits == 16U) ? (uint16_t)(segs[i].len / 2U) : segs[i].len;
        if (units == 0U ||
            HAL_SPI_TransmitReceive(s_spi.hspi, (uint8_t *)segs[i].tx,
                                    segs[i].rx, units, ENC_XFER_TIMEOUT_MS) != HAL_OK)
        {
            enc_cs(type, false);
            return false;
        }
    }
    enc_cs(type, false);

    *ts_ms = bsp_time_ms(); // 时间戳统一走 bsp
    return true;
}

void enc_spi_abort(eEncoderType type)
{
    enc_cs(type, false);
}
