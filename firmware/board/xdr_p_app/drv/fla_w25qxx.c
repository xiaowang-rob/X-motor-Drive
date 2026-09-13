// ============================================================
// w25qxx.c — W25Q128 串行 NOR Flash 驱动
//
// 芯片协议（8bit SPI，Mode0/3 均支持）：页编程 256B / 扇区擦除 4KB /
// 每组 16个扇区，总共256组，分组管理
// 容量 16MB；操作前写使能(0x06)，完成后轮询状态寄存器 BUSY。
// 底层直接使用本板 Flash SPI（platform.h 的 FLASH_HSPI/hspi2）。
// ============================================================
#include "flash_drivers.h"

#include "spi.h"

// ---------- Flash SPI + CS ----------

#define W25_CAPACITY_BYTES (16U * 1024U * 1024U)
#define W25_PAGE_SIZE 256U
#define W25_SECTOR_SIZE (4U * 1024U)
#define W25_SECTOR_COUNT (W25_CAPACITY_BYTES / W25_SECTOR_SIZE)

#define FCMD_READ_ID 0x9FU      // 读ID指令
#define FCMD_WRITE_ENABLE 0x06U // 写使能指令
#define FCMD_READ_STATUS 0x05U  // 读状态寄存器指令
#define FCMD_ERASE_SECTOR 0x20U // 扇区擦除指令
#define FCMD_READ_DATA 0x03U    // 读数据指令
#define FCMD_WRITE_PAGE 0x02U   // 写页指令

#define FBIT_SR_BUSY 0x01U   // 状态寄存器 BUSY 位
#define FTIMEOUT_OP_MS 1000U // 操作超时（ms）
#define READ_CHUNK 32U       // 读取数据时的分块大小（字节）

#define BLOCK_SECTOR_NUM 16U // 每个块对应16个扇区
static const uint8_t W25_JEDEC_ID[3] = {0xEFU, 0x40U, 0x18U};

// 把扇区分为256块 每个块对应16个扇区 每次分配按照一个块分配 然后让所有块均匀磨损
// TODO: 暂时先按照一个块分配 后续添加均匀磨损
static const uint32_t W25_SECTOR_BOUNDS[BLOCK_SECTOR_NUM + 1] = {
    0x00000000U,
    0x00001000U,
    0x00002000U,
    0x00003000U,
    0x00004000U,
    0x00005000U,
    0x00006000U,
    0x00007000U,
    0x00008000U,
    0x00009000U,
    0x0000A000U,
    0x0000B000U,
    0x0000C000U,
    0x0000D000U,
    0x0000E000U,
    0x0000F000U,
    0x00010000U}; // 最后一个地址是为了方便计算
// ---- 实例 handle ----

typedef struct
{
    const tFlashDriverOps *ops; // 该实例的操作表
    SPI_HandleTypeDef *hspi;    // 外设：SPI
    GPIO_TypeDef *cs_port;      // 配置：片选端口
    uint16_t cs_pin;            // 配置：片选引脚
    uint8_t tx_buf[4U + W25_PAGE_SIZE];
    uint8_t rx_buf[4U + W25_PAGE_SIZE];
    uint8_t rd_ff[READ_CHUNK];
} tW25Qxx;

// ---- 底层原语（CS 由调用处控制；外设/引脚取自实例） ----

static void fl_cs(tW25Qxx *ctx, bool active)
{
    HAL_GPIO_WritePin(ctx->cs_port, ctx->cs_pin,
                      active ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static bool fl_xfer(tW25Qxx *ctx, const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    if (!ctx || !tx || !rx || len == 0U)
        return false;
    if (HAL_SPI_GetState(ctx->hspi) != HAL_SPI_STATE_READY)
        return false;
    return HAL_SPI_TransmitReceive(ctx->hspi, (uint8_t *)tx, rx, len, 100U) == HAL_OK;
}

static bool w25_tx(tW25Qxx *ctx, uint16_t len)
{
    return fl_xfer(ctx, ctx->tx_buf, ctx->rx_buf, len);
}

// 读取状态寄存器（BUSY 位）----
static uint8_t w25_read_sr(tW25Qxx *ctx)
{
    ctx->tx_buf[0] = FCMD_READ_STATUS;
    ctx->tx_buf[1] = 0xFFU;
    fl_cs(ctx, true);
    bool ok = fl_xfer(ctx, ctx->tx_buf, ctx->rx_buf, 2U);
    fl_cs(ctx, false);
    return ok ? ctx->rx_buf[1] : 0xFFU;
}

// 等待操作完成（BUSY 位清零）
// 优化：读一次状态寄存器要发 2 字节 SPI，轮询太密纯属白占总线；
//       改为"读状态 → 短延时"节奏，且每轮只取一次 tick。
// 注：仅在主循环上下文调用（HAL_Delay 依赖 SysTick，不可在 ISR 内使用）。
static bool w25_wait_idle(tW25Qxx *ctx, uint32_t timeout_ms)
{
    uint32_t t0 = HAL_GetTick();
    for (;;)
    {
        if ((w25_read_sr(ctx) & FBIT_SR_BUSY) == 0U)
            return true;
        HAL_Delay(1U); // 擦除 ~45ms / 页编程 ~0.7ms，1ms 粒度足够
        if ((HAL_GetTick() - t0) >= timeout_ms)
            return false;
    }
}
// 写使能（写使能指令）----
static bool w25_write_enable(tW25Qxx *ctx)
{
    ctx->tx_buf[0] = FCMD_WRITE_ENABLE;
    ctx->tx_buf[1] = 0xFFU;
    fl_cs(ctx, true);
    bool ok = fl_xfer(ctx, ctx->tx_buf, ctx->rx_buf, 2U);
    fl_cs(ctx, false);
    return ok;
}

// ---- ops 实现 ----

static bool w25_init(FlashChipHandle h)
{
    tW25Qxx *ctx = (tW25Qxx *)h;
    if (!ctx)
        return false;

    // 配置 Flash SPI
    // 但是这个在 CubeMX 中已经配置了，这里不需要再配置

    for (uint8_t attempt = 0U; attempt < 5U; attempt++)
    {
        ctx->tx_buf[0] = FCMD_READ_ID;
        ctx->tx_buf[1] = 0xFFU;
        ctx->tx_buf[2] = 0xFFU;
        ctx->tx_buf[3] = 0xFFU;

        fl_cs(ctx, true);
        bool ok = fl_xfer(ctx, ctx->tx_buf, ctx->rx_buf, 4U);
        fl_cs(ctx, false);

        if (ok && ctx->rx_buf[1] == W25_JEDEC_ID[0] &&
            ctx->rx_buf[2] == W25_JEDEC_ID[1] &&
            ctx->rx_buf[3] == W25_JEDEC_ID[2])
        {
            return true;
        }
        HAL_Delay(10U);
    }

    return false;
}
// 读取数据（读数据指令）----
static bool w25_read(FlashChipHandle h, uint32_t addr, uint8_t *data, uint32_t len)
{
    tW25Qxx *ctx = (tW25Qxx *)h;
    if (!ctx || !data || (addr + len) > W25_CAPACITY_BYTES)
        return false;

    ctx->tx_buf[0] = FCMD_READ_DATA;
    ctx->tx_buf[1] = (uint8_t)(addr >> 16);
    ctx->tx_buf[2] = (uint8_t)(addr >> 8);
    ctx->tx_buf[3] = (uint8_t)addr;

    fl_cs(ctx, true);
    if (!w25_tx(ctx, 4U))
    {
        fl_cs(ctx, false);
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
        if (!fl_xfer(ctx, ctx->rd_ff, data + done, (uint16_t)n))
        {
            fl_cs(ctx, false);
            return false;
        }
        done += n;
    }

    fl_cs(ctx, false);
    return true;
}

// 页编程（写页指令）----
static bool w25_page_program(tW25Qxx *ctx, uint32_t addr, const uint8_t *data, uint16_t len)
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

    fl_cs(ctx, true);
    bool ok = fl_xfer(ctx, ctx->tx_buf, ctx->rx_buf, (uint16_t)(4U + len));
    fl_cs(ctx, false);

    if (!ok)
        return false;
    return w25_wait_idle(ctx, FTIMEOUT_OP_MS);
}

// 按地址写入-自动换页
static bool w25_write(FlashChipHandle h, uint32_t addr, const uint8_t *data, uint32_t len)
{
    tW25Qxx *ctx = (tW25Qxx *)h;
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
            return false;
        }
        done += n;
    }

    return true;
}

// 扇区擦除（扇区擦除指令）----
static bool w25_erase_sector_at(tW25Qxx *ctx, uint32_t sector_addr)
{
    if (!w25_write_enable(ctx))
        return false;

    ctx->tx_buf[0] = FCMD_ERASE_SECTOR;
    ctx->tx_buf[1] = (uint8_t)(sector_addr >> 16);
    ctx->tx_buf[2] = (uint8_t)(sector_addr >> 8);
    ctx->tx_buf[3] = (uint8_t)sector_addr;

    fl_cs(ctx, true);
    bool ok = fl_xfer(ctx, ctx->tx_buf, ctx->rx_buf, 4U);
    fl_cs(ctx, false);

    if (!ok)
        return false;
    return w25_wait_idle(ctx, FTIMEOUT_OP_MS);
}

// 扇区擦除（按地址擦除）----
static bool w25_erase(FlashChipHandle h, uint32_t addr, uint32_t len)
{
    tW25Qxx *ctx = (tW25Qxx *)h;
    if (!ctx || len == 0U || (addr + len) > W25_CAPACITY_BYTES)
        return false;

    uint32_t first = addr >> 12;
    uint32_t end = (addr + len + W25_SECTOR_SIZE - 1U) >> 12;

    for (uint32_t i = first; i < end; i++)
    {
        if (!w25_erase_sector_at(ctx, i << 12))
        {
            return false;
        }
    }

    return true;
}

static bool w25_erase_sector(FlashChipHandle h, uint8_t sec_id)
{
    tW25Qxx *ctx = (tW25Qxx *)h;
    if (!ctx || sec_id >= BLOCK_SECTOR_NUM)
        return false;
    return w25_erase_sector_at(ctx, W25_SECTOR_BOUNDS[sec_id]);
}
static uint8_t w25_get_sector_count(FlashChipHandle h)
{
    tW25Qxx *ctx = (tW25Qxx *)h;
    if (!ctx)
        return 0U;
    return BLOCK_SECTOR_NUM;
}
static uint32_t w25_get_sector_addr(FlashChipHandle h, uint8_t sec_id)
{
    tW25Qxx *ctx = (tW25Qxx *)h;
    if (!ctx || sec_id >= BLOCK_SECTOR_NUM)
        return 0U;
    return W25_SECTOR_BOUNDS[sec_id];
}
static uint32_t w25_get_sector_size(FlashChipHandle h, uint8_t sec_id)
{
    tW25Qxx *ctx = (tW25Qxx *)h;
    if (!ctx || sec_id >= BLOCK_SECTOR_NUM)
        return 0U;
    return W25_SECTOR_SIZE;
}

const tFlashDriverOps w25qxx_driver_ops = {
    .init = w25_init,
    .read = w25_read,
    .write = w25_write,
    .erase_addr = w25_erase,
    .erase_sector = w25_erase_sector,
    .get_sector_count = w25_get_sector_count,
    .get_sector_addr = w25_get_sector_addr,
    .get_sector_size = w25_get_sector_size,
};

// 静态实例（外部 SPI NOR：W25Q128）
static tW25Qxx s_w25 = {
    .ops = &w25qxx_driver_ops,
    .hspi = &hspi2,
    .cs_port = GPIOB,
    .cs_pin = GPIO_PIN_12,
};

FlashChipHandle w25_get_handle(void)
{
    return (FlashChipHandle)&s_w25;
}
