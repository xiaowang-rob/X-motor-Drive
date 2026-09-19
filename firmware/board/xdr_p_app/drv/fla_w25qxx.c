// ============================================================
// fla_w25qxx.c — W25Q128 串行 NOR Flash 驱动（板级，直连 HAL）
//
// 芯片协议（8bit SPI，Mode0/3 均支持）：页编程 256B / 扇区擦除 4KB /
// 每组 16 个扇区共 256 组，分组管理；容量 16MB。
// 操作前写使能(0x06)，完成后轮询状态寄存器 BUSY。
//
// 实例形态：文件内静态实例（外设 SPI / CS / 收发缓冲），无 ops、无堆。
// 时间统一走 bsp_time（HAL_Delay 依赖 SysTick，仅主循环上下文可用）。
// 本文件实现 app/abs/flash_board.h 的 W25Qxx 路由。
// ============================================================
#include "fla_w25qxx.h"

#include "bsp_time.h"

#include "spi.h"

// ---------- Flash SPI + CS ----------
#define W25_CAPACITY_BYTES (16U * 1024U * 1024U)
#define W25_PAGE_SIZE 256U
#define W25_SECTOR_SIZE (4U * 1024U)

#define FCMD_READ_ID 0x9FU      // 读 ID 指令
#define FCMD_WRITE_ENABLE 0x06U // 写使能指令
#define FCMD_READ_STATUS 0x05U  // 读状态寄存器指令
#define FCMD_ERASE_SECTOR 0x20U // 扇区擦除指令
#define FCMD_READ_DATA 0x03U    // 读数据指令
#define FCMD_WRITE_PAGE 0x02U   // 写页指令

#define FBIT_SR_BUSY 0x01U   // 状态寄存器 BUSY 位
#define FTIMEOUT_OP_MS 1000U // 操作超时（ms）
#define READ_CHUNK 32U       // 读取分块大小（字节）

#define BLOCK_SECTOR_NUM 16U // 用户区扇区数（每块 16 个扇区）
static const uint8_t W25_JEDEC_ID[3] = {0xEFU, 0x40U, 0x18U};

// 把扇区分为 256 块，每块 16 个扇区，每次按一个块分配以便均匀磨损
// TODO: 暂时先按照一个块分配，后续添加均匀磨损
static const uint32_t W25_SECTOR_BOUNDS[BLOCK_SECTOR_NUM + 1] = {
    0x00000000U, 0x00001000U, 0x00002000U, 0x00003000U,
    0x00004000U, 0x00005000U, 0x00006000U, 0x00007000U,
    0x00008000U, 0x00009000U, 0x0000A000U, 0x0000B000U,
    0x0000C000U, 0x0000D000U, 0x0000E000U, 0x0000F000U,
    0x00010000U, // 末尾哨兵，方便计算
};

// ---------- 实例 ----------
typedef struct
{
    SPI_HandleTypeDef *hspi; // 外设：SPI
    GPIO_TypeDef *cs_port;   // 配置：片选端口
    uint16_t cs_pin;         // 配置：片选引脚
    uint8_t tx_buf[4U + W25_PAGE_SIZE];
    uint8_t rx_buf[4U + W25_PAGE_SIZE];
    uint8_t rd_ff[READ_CHUNK];
} tW25Qxx;

static tW25Qxx s_w25 = {
    .hspi = &hspi2,
    .cs_port = GPIOB,
    .cs_pin = GPIO_PIN_12,
};

// ---------- 底层原语（CS 由调用处控制） ----------

static void w25_cs(bool active)
{
    HAL_GPIO_WritePin(s_w25.cs_port, s_w25.cs_pin,
                      active ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static bool w25_xfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    if (!tx || !rx || len == 0U)
        return false;
    if (HAL_SPI_GetState(s_w25.hspi) != HAL_SPI_STATE_READY)
        return false;
    return HAL_SPI_TransmitReceive(s_w25.hspi, (uint8_t *)tx, rx, len, 100U) == HAL_OK;
}

static bool w25_tx(uint16_t len)
{
    return w25_xfer(s_w25.tx_buf, s_w25.rx_buf, len);
}

// 读状态寄存器（BUSY 位）
static uint8_t w25_read_sr(void)
{
    s_w25.tx_buf[0] = FCMD_READ_STATUS;
    s_w25.tx_buf[1] = 0xFFU;
    w25_cs(true);
    bool ok = w25_xfer(s_w25.tx_buf, s_w25.rx_buf, 2U);
    w25_cs(false);
    return ok ? s_w25.rx_buf[1] : 0xFFU;
}

// 等待操作完成（BUSY 位清零）
// 优化：读一次状态寄存器要发 2 字节 SPI，轮询太密纯属白占总线；
//       改为"读状态 → 短延时"节奏，且每轮只取一次 tick。
// 注：仅在主循环上下文调用（延时依赖 SysTick）。
static bool w25_wait_idle(uint32_t timeout_ms)
{
    uint32_t t0 = bsp_time_ms();
    for (;;)
    {
        if ((w25_read_sr() & FBIT_SR_BUSY) == 0U)
            return true;
        bsp_time_delay_ms(1U); // 擦除 ~45ms / 页编程 ~0.7ms，1ms 粒度足够
        if ((bsp_time_ms() - t0) >= timeout_ms)
            return false;
    }
}

// 写使能
static bool w25_write_enable(void)
{
    s_w25.tx_buf[0] = FCMD_WRITE_ENABLE;
    s_w25.tx_buf[1] = 0xFFU;
    w25_cs(true);
    bool ok = w25_xfer(s_w25.tx_buf, s_w25.rx_buf, 2U);
    w25_cs(false);
    return ok;
}

// ---------- 板级钩子实现（W25Qxx 路由） ----------

bool fla_w25_open(void)
{
    // SPI 已由 CubeMX 配置（Mode0/3）；这里做 JEDEC ID 连接校验
    for (uint8_t attempt = 0U; attempt < 5U; attempt++)
    {
        s_w25.tx_buf[0] = FCMD_READ_ID;
        s_w25.tx_buf[1] = 0xFFU;
        s_w25.tx_buf[2] = 0xFFU;
        s_w25.tx_buf[3] = 0xFFU;

        w25_cs(true);
        bool ok = w25_xfer(s_w25.tx_buf, s_w25.rx_buf, 4U);
        w25_cs(false);

        if (ok && s_w25.rx_buf[1] == W25_JEDEC_ID[0] &&
            s_w25.rx_buf[2] == W25_JEDEC_ID[1] &&
            s_w25.rx_buf[3] == W25_JEDEC_ID[2])
            return true;

        bsp_time_delay_ms(10U);
    }
    return false;
}

bool fla_w25_read(uint32_t addr, uint8_t *data, uint32_t len)
{
    if (!data || len == 0U || (addr + len) > W25_CAPACITY_BYTES)
        return false;

    s_w25.tx_buf[0] = FCMD_READ_DATA;
    s_w25.tx_buf[1] = (uint8_t)(addr >> 16);
    s_w25.tx_buf[2] = (uint8_t)(addr >> 8);
    s_w25.tx_buf[3] = (uint8_t)addr;

    w25_cs(true);
    if (!w25_tx(4U))
    {
        w25_cs(false);
        return false;
    }

    for (uint8_t i = 0U; i < READ_CHUNK; i++)
        s_w25.rd_ff[i] = 0xFFU;

    uint32_t done = 0U;
    while (done < len)
    {
        uint32_t n = len - done;
        if (n > READ_CHUNK)
            n = READ_CHUNK;
        if (!w25_xfer(s_w25.rd_ff, data + done, (uint16_t)n))
        {
            w25_cs(false);
            return false;
        }
        done += n;
    }

    w25_cs(false);
    return true;
}

// 页编程（单页，len ≤ 256）
static bool w25_page_program(uint32_t addr, const uint8_t *data, uint16_t len)
{
    if (len == 0U || len > W25_PAGE_SIZE)
        return false;

    if (!w25_write_enable())
        return false;

    s_w25.tx_buf[0] = FCMD_WRITE_PAGE;
    s_w25.tx_buf[1] = (uint8_t)(addr >> 16);
    s_w25.tx_buf[2] = (uint8_t)(addr >> 8);
    s_w25.tx_buf[3] = (uint8_t)addr;
    for (uint16_t i = 0U; i < len; i++)
        s_w25.tx_buf[4U + i] = data[i];

    w25_cs(true);
    bool ok = w25_xfer(s_w25.tx_buf, s_w25.rx_buf, (uint16_t)(4U + len));
    w25_cs(false);

    if (!ok)
        return false;
    return w25_wait_idle(FTIMEOUT_OP_MS);
}

bool fla_w25_write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    if (!data || len == 0U || (addr + len) > W25_CAPACITY_BYTES)
        return false;

    uint32_t done = 0U;
    while (done < len)
    {
        uint32_t page_left = W25_PAGE_SIZE - ((addr + done) % W25_PAGE_SIZE);
        uint32_t n = len - done;
        if (n > page_left)
            n = page_left;

        if (!w25_page_program(addr + done, data + done, (uint16_t)n))
            return false;
        done += n;
    }

    return true;
}

// 单个扇区擦除（4KB）
static bool w25_erase_sector_at(uint32_t sector_addr)
{
    if (!w25_write_enable())
        return false;

    s_w25.tx_buf[0] = FCMD_ERASE_SECTOR;
    s_w25.tx_buf[1] = (uint8_t)(sector_addr >> 16);
    s_w25.tx_buf[2] = (uint8_t)(sector_addr >> 8);
    s_w25.tx_buf[3] = (uint8_t)sector_addr;

    w25_cs(true);
    bool ok = w25_xfer(s_w25.tx_buf, s_w25.rx_buf, 4U);
    w25_cs(false);

    if (!ok)
        return false;
    return w25_wait_idle(FTIMEOUT_OP_MS);
}

bool fla_w25_erase_addr(uint32_t addr, uint32_t len)
{
    if (len == 0U || (addr + len) > W25_CAPACITY_BYTES)
        return false;

    uint32_t first = addr >> 12;
    uint32_t end = (addr + len + W25_SECTOR_SIZE - 1U) >> 12;

    for (uint32_t i = first; i < end; i++)
    {
        if (!w25_erase_sector_at(i << 12))
            return false;
    }
    return true;
}

bool fla_w25_erase_sector(uint8_t sec_id)
{
    if (sec_id >= BLOCK_SECTOR_NUM)
        return false;
    return w25_erase_sector_at(W25_SECTOR_BOUNDS[sec_id]);
}

uint8_t fla_w25_sector_count(void)
{
    return BLOCK_SECTOR_NUM;
}

uint32_t fla_w25_sector_addr(uint8_t sec_id)
{
    if (sec_id >= BLOCK_SECTOR_NUM)
        return 0U;
    return W25_SECTOR_BOUNDS[sec_id];
}

uint32_t fla_w25_sector_size(uint8_t sec_id)
{
    if (sec_id >= BLOCK_SECTOR_NUM)
        return 0U;
    return W25_SECTOR_SIZE;
}
