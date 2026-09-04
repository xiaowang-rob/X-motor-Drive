// ============================================================
// w25qxx.c — W25Q128 串行 NOR Flash 驱动（usr/drv，v2 直连版，同步）
//
// 芯片协议（8bit SPI，Mode0/3 均支持）：页编程 256B / 扇区擦除 4KB /
// 容量 16MB；操作前写使能(0x06)，完成后轮询状态寄存器 BUSY。
// 底层直接使用本板 Flash SPI（platform.h 的 FLASH_SPI_CH/hspi2）。
// ============================================================

#include <stdlib.h>

#include "usr/abs/device.h"
#include "usr/abs/flash.h"

#include "platform.h"
#include "flash_drivers.h"

#define W25_CAPACITY_BYTES (16U * 1024U * 1024U)
#define W25_PAGE_SIZE 256U
#define W25_SECTOR_SIZE (4U * 1024U)
#define W25_SECTOR_COUNT (W25_CAPACITY_BYTES / W25_SECTOR_SIZE)

#define FCMD_READ_ID 0x9FU
#define FCMD_WRITE_ENABLE 0x06U
#define FCMD_READ_STATUS 0x05U
#define FCMD_ERASE_SECTOR 0x20U
#define FCMD_READ_DATA 0x03U
#define FCMD_WRITE_PAGE 0x02U

#define FBIT_SR_BUSY 0x01U
#define FTIMEOUT_OP_MS 1000U
#define READ_CHUNK 32U

static const uint8_t W25_JEDEC_ID[3] = {0xEFU, 0x40U, 0x18U};

// ---- 本板 Flash SPI（platform.h） ----

static void fl_cs(bool active)
{
    HAL_GPIO_WritePin(FLASH_CS_GPIOx, FLASH_CS_GPIOx_PIN,
                      active ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static bool fl_xfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    if (!tx || !rx || len == 0U)
        return false;
    if (HAL_SPI_GetState(&FLASH_SPI_CH) != HAL_SPI_STATE_READY)
        return false;
    return HAL_SPI_TransmitReceive(&FLASH_SPI_CH, (uint8_t *)tx, rx, len, 100U) == HAL_OK;
}

// 按 W25Q 需要显式配置 Flash SPI（8bit / Mode3）
static bool fl_spi_cfg(void)
{
    SPI_HandleTypeDef *h = &FLASH_SPI_CH;
    h->Init.Mode = SPI_MODE_MASTER;
    h->Init.Direction = SPI_DIRECTION_2LINES;
    h->Init.DataSize = SPI_DATASIZE_8BIT;
    h->Init.CLKPolarity = SPI_POLARITY_HIGH;
    h->Init.CLKPhase = SPI_PHASE_2EDGE;
    h->Init.NSS = SPI_NSS_SOFT;
    h->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
    h->Init.FirstBit = SPI_FIRSTBIT_MSB;
    h->Init.TIMode = SPI_TIMODE_DISABLE;
    h->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    h->Init.CRCPolynomial = 10;
    return HAL_SPI_Init(h) == HAL_OK;
}

// ---- 驱动上下文 ----

typedef struct
{
    eDeviceStatus dstate;
    uint8_t tx_buf[4U + W25_PAGE_SIZE];
    uint8_t rx_buf[4U + W25_PAGE_SIZE];
    uint8_t rd_ff[READ_CHUNK];
} tW25Qxx_ctx;

// ---- 底层原语（CS 由调用处控制） ----

static bool w25_tx(tW25Qxx_ctx *ctx, uint16_t len)
{
    return fl_xfer(ctx->tx_buf, ctx->rx_buf, len);
}

static uint8_t w25_read_sr(tW25Qxx_ctx *ctx)
{
    ctx->tx_buf[0] = FCMD_READ_STATUS;
    ctx->tx_buf[1] = 0xFFU;
    fl_cs(true);
    bool ok = fl_xfer(ctx->tx_buf, ctx->rx_buf, 2U);
    fl_cs(false);
    return ok ? ctx->rx_buf[1] : 0xFFU;
}

static bool w25_wait_idle(tW25Qxx_ctx *ctx, uint32_t timeout_ms)
{
    uint32_t t0 = platform_get_ms();
    while (w25_read_sr(ctx) & FBIT_SR_BUSY)
    {
        if ((platform_get_ms() - t0) >= timeout_ms)
            return false;
    }
    return true;
}

static bool w25_write_enable(tW25Qxx_ctx *ctx)
{
    ctx->tx_buf[0] = FCMD_WRITE_ENABLE;
    ctx->tx_buf[1] = 0xFFU;
    fl_cs(true);
    bool ok = fl_xfer(ctx->tx_buf, ctx->rx_buf, 2U);
    fl_cs(false);
    return ok;
}

// ---- ops 实现 ----

static bool w25_init(FlashChipHandle h)
{
    tW25Qxx_ctx *ctx = (tW25Qxx_ctx *)h;
    if (!ctx)
        return false;

    if (!fl_spi_cfg())
        return false;

    for (uint8_t attempt = 0U; attempt < 5U; attempt++)
    {
        ctx->tx_buf[0] = FCMD_READ_ID;
        ctx->tx_buf[1] = 0xFFU;
        ctx->tx_buf[2] = 0xFFU;
        ctx->tx_buf[3] = 0xFFU;

        fl_cs(true);
        bool ok = fl_xfer(ctx->tx_buf, ctx->rx_buf, 4U);
        fl_cs(false);

        if (ok && ctx->rx_buf[1] == W25_JEDEC_ID[0] &&
            ctx->rx_buf[2] == W25_JEDEC_ID[1] &&
            ctx->rx_buf[3] == W25_JEDEC_ID[2])
        {
            ctx->dstate = DEV_ONLINE;
            return true;
        }
        platform_delay_ms(10U);
    }

    ctx->dstate = DEV_OFFLINE;
    return false;
}

static bool w25_read(FlashChipHandle h, uint32_t addr, uint8_t *data, uint32_t len)
{
    tW25Qxx_ctx *ctx = (tW25Qxx_ctx *)h;
    if (!ctx || !data || (addr + len) > W25_CAPACITY_BYTES)
        return false;

    ctx->tx_buf[0] = FCMD_READ_DATA;
    ctx->tx_buf[1] = (uint8_t)(addr >> 16);
    ctx->tx_buf[2] = (uint8_t)(addr >> 8);
    ctx->tx_buf[3] = (uint8_t)addr;

    fl_cs(true);
    if (!w25_tx(ctx, 4U))
    {
        fl_cs(false);
        ctx->dstate = DEV_RUN_ERROR;
        return false;
    }

    for (uint8_t i = 0U; i < READ_CHUNK; i++)
        ctx->rd_ff[i] = 0xFFU;

    uint32_t done = 0U;
    while (done < len)
    {
        uint32_t n = len - done;
        if (n > READ_CHUNK)
            n = READ_CHUNK;
        if (!fl_xfer(ctx->rd_ff, data + done, (uint16_t)n))
        {
            fl_cs(false);
            ctx->dstate = DEV_RUN_ERROR;
            return false;
        }
        done += n;
    }

    fl_cs(false);
    ctx->dstate = DEV_RUNNING;
    return true;
}

static bool w25_page_program(tW25Qxx_ctx *ctx, uint32_t addr, const uint8_t *data, uint16_t len)
{
    if (len == 0U || len > W25_PAGE_SIZE)
        return false;

    if (!w25_write_enable(ctx))
        return false;

    ctx->tx_buf[0] = FCMD_WRITE_PAGE;
    ctx->tx_buf[1] = (uint8_t)(addr >> 16);
    ctx->tx_buf[2] = (uint8_t)(addr >> 8);
    ctx->tx_buf[3] = (uint8_t)addr;
    for (uint16_t i = 0U; i < len; i++)
        ctx->tx_buf[4U + i] = data[i];

    fl_cs(true);
    bool ok = fl_xfer(ctx->tx_buf, ctx->rx_buf, (uint16_t)(4U + len));
    fl_cs(false);

    if (!ok)
        return false;
    return w25_wait_idle(ctx, FTIMEOUT_OP_MS);
}

static bool w25_write(FlashChipHandle h, uint32_t addr, const uint8_t *data, uint32_t len)
{
    tW25Qxx_ctx *ctx = (tW25Qxx_ctx *)h;
    if (!ctx || !data || (addr + len) > W25_CAPACITY_BYTES)
        return false;

    uint32_t done = 0U;
    while (done < len)
    {
        uint32_t page_left = W25_PAGE_SIZE - ((addr + done) % W25_PAGE_SIZE);
        uint32_t n = len - done;
        if (n > page_left)
            n = page_left;

        if (!w25_page_program(ctx, addr + done, data + done, (uint16_t)n))
        {
            ctx->dstate = DEV_RUN_ERROR;
            return false;
        }
        done += n;
    }

    ctx->dstate = DEV_RUNNING;
    return true;
}

static bool w25_erase_sector_at(tW25Qxx_ctx *ctx, uint32_t sector_addr)
{
    if (!w25_write_enable(ctx))
        return false;

    ctx->tx_buf[0] = FCMD_ERASE_SECTOR;
    ctx->tx_buf[1] = (uint8_t)(sector_addr >> 16);
    ctx->tx_buf[2] = (uint8_t)(sector_addr >> 8);
    ctx->tx_buf[3] = (uint8_t)sector_addr;

    fl_cs(true);
    bool ok = fl_xfer(ctx->tx_buf, ctx->rx_buf, 4U);
    fl_cs(false);

    if (!ok)
        return false;
    return w25_wait_idle(ctx, FTIMEOUT_OP_MS);
}

static bool w25_erase(FlashChipHandle h, uint32_t addr, uint32_t len)
{
    tW25Qxx_ctx *ctx = (tW25Qxx_ctx *)h;
    if (!ctx || len == 0U || (addr + len) > W25_CAPACITY_BYTES)
        return false;

    uint32_t first = addr >> 12;
    uint32_t end = (addr + len + W25_SECTOR_SIZE - 1U) >> 12;

    for (uint32_t i = first; i < end; i++)
    {
        if (!w25_erase_sector_at(ctx, i << 12))
        {
            ctx->dstate = DEV_RUN_ERROR;
            return false;
        }
    }

    ctx->dstate = DEV_RUNNING;
    return true;
}

static uint32_t w25_get_capacity(FlashChipHandle h)
{
    (void)h;
    return W25_CAPACITY_BYTES;
}

static uint32_t w25_get_page_size(FlashChipHandle h)
{
    (void)h;
    return W25_PAGE_SIZE;
}

static uint32_t w25_get_sector_size(FlashChipHandle h)
{
    (void)h;
    return W25_SECTOR_SIZE;
}

static uint8_t w25_get_state(FlashChipHandle h)
{
    tW25Qxx_ctx *ctx = (tW25Qxx_ctx *)h;
    return (uint8_t)(ctx ? ctx->dstate : DEV_OFFLINE);
}

const tFlashDriverOps w25qxx_driver_ops = {
    .init = w25_init,
    .read = w25_read,
    .write = w25_write,
    .erase = w25_erase,
    .get_capacity = w25_get_capacity,
    .get_page_size = w25_get_page_size,
    .get_sector_size = w25_get_sector_size,
    .get_state = w25_get_state,
};

FlashChipHandle w25qxx_create(void)
{
    tW25Qxx_ctx *ctx = (tW25Qxx_ctx *)calloc(1U, sizeof(tW25Qxx_ctx));
    if (!ctx)
        return NULL;
    ctx->dstate = DEV_OFFLINE;
    return (FlashChipHandle)ctx;
}

void w25qxx_destroy(FlashChipHandle h)
{
    free(h);
}
