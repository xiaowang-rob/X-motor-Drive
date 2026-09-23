// ============================================================
// fla_mcu.c — 内部 MCU Flash 介质驱动（板级，直连 HAL）
//
// STM32G474CB，128KB Flash（64 页 × 2KB）：
//   - read  ：直接内存读（存储器映射）
//   - write ：按 double-word(64bit) 编程（G4 只能 64bit 对齐编程，
//             F4 的 byte/word 编程在 G4 上不存在）
//   - erase ：按页擦除（FLASH_TYPEERASE_PAGES），并按 bank 边界自动拆分
//             （G474 若 DBANK=1 则页号在 bank 内重新计数）
// ============================================================
#include "fla_mcu.h"

#include "stm32g4xx_hal.h" // FLASH 编程接口（HAL 总头按 conf 决定是否展开）

#define FLASH_START_ADDR 0x08000000U                          // Flash 起始地址
#define FLASH_CAPACITY (128U * 1024U)                         // Flash 容量 (bytes)
#define FLASH_PAGE_BYTES 2048U                                // 页大小（G4：2KB）
#define FLASH_TOTAL_PAGES (FLASH_CAPACITY / FLASH_PAGE_BYTES) // 64 页

#define MCU_NUM_PAGE_BL 24U  // Bootloader 页数
#define MCU_NUM_PAGE_APP 37U // App 页数
#define MCU_NUM_PAGE_USR 3U  // 用户数据页数

#define BL_FIRST_PAGE 0U
#define APP_FIRST_PAGE (BL_FIRST_PAGE + MCU_NUM_PAGE_BL)
#define USR_FIRST_PAGE (APP_FIRST_PAGE + MCU_NUM_PAGE_APP)

#define PAGE_ADDR(p) (FLASH_START_ADDR + (p) * FLASH_PAGE_BYTES)

// 分区几何（编译期常量）
#define BL_START_ADDR PAGE_ADDR(BL_FIRST_PAGE)
#define APP_START_ADDR PAGE_ADDR(APP_FIRST_PAGE)
#define USR_START_ADDR PAGE_ADDR(USR_FIRST_PAGE)
#define BL_SIZE (APP_START_ADDR - BL_START_ADDR)
#define APP_SIZE (USR_START_ADDR - APP_START_ADDR)

// ---------- 实例 ----------
struct tFlaMcu
{
    uint32_t start_addr; // 介质起始地址
    uint32_t end_addr;   // 介质结束地址（含）
    bool inited;
};

tFlaMcu g_fla_mcu = {
    .start_addr = FLASH_START_ADDR,
    .end_addr = FLASH_START_ADDR + FLASH_CAPACITY - 1U,
    .inited = false,
};

// ---------- bank/页换算（双 bank 器件页号在 bank 内重新计数） ----------
static uint32_t pages_per_bank(void)
{
#if defined(FLASH_OPTR_DBANK)
    if (READ_BIT(FLASH->OPTR, FLASH_OPTR_DBANK) != 0U)
        return FLASH_TOTAL_PAGES / 2U;
#endif
    return FLASH_TOTAL_PAGES;
}

static void page_to_bank_page(uint32_t page, uint32_t *bank, uint32_t *bank_page)
{
#if defined(FLASH_OPTR_DBANK)
    if (READ_BIT(FLASH->OPTR, FLASH_OPTR_DBANK) != 0U)
    {
        uint32_t per_bank = FLASH_TOTAL_PAGES / 2U;
        if (page < per_bank)
        {
            *bank = FLASH_BANK_1;
            *bank_page = page;
        }
        else
        {
            *bank = FLASH_BANK_2;
            *bank_page = page - per_bank;
        }
        return;
    }
#endif
    *bank = FLASH_BANK_1;
    *bank_page = page;
}

// ---------- 地址 → 页索引（0..63）；越界返回 -1 ----------
static int page_index_at(uint32_t addr)
{
    if (addr < FLASH_START_ADDR || addr > (FLASH_START_ADDR + FLASH_CAPACITY - 1U))
        return -1;
    return (int)((addr - FLASH_START_ADDR) / FLASH_PAGE_BYTES);
}

// 擦除连续的 n 页（起始页 first_page）；按 bank 边界自动拆分
static bool flash_erase_pages(uint32_t first_page, uint32_t n)
{
    uint32_t per_bank = pages_per_bank();

    while (n > 0U)
    {
        uint32_t bank, bank_page;
        page_to_bank_page(first_page, &bank, &bank_page);

        uint32_t room = per_bank - bank_page; // 本 bank 内还能连擦多少页
        uint32_t chunk = (n < room) ? n : room;

        FLASH_EraseInitTypeDef erase = {0};
        uint32_t page_err = 0U;
        erase.TypeErase = FLASH_TYPEERASE_PAGES;
        erase.Banks = bank;
        erase.Page = bank_page;
        erase.NbPages = chunk;
        if (HAL_FLASHEx_Erase(&erase, &page_err) != HAL_OK)
            return false;

        first_page += chunk;
        n -= chunk;
    }
    return true;
}

// ---------- 驱动接口（tFlashOps） ----------

static bool fla_mcu_open(void *handle)
{
    tFlaMcu *inst = (tFlaMcu *)handle;
    if (!inst)
        return false;
    inst->inited = true;
    return true; // 内部 Flash 无需初始化
}

static bool fla_mcu_read(void *handle, uint32_t addr, uint8_t *data, uint32_t len)
{
    const tFlaMcu *inst = (const tFlaMcu *)handle;
    if (!inst || !data || len == 0U)
        return false;
    if (addr < inst->start_addr || (addr + len - 1U) > inst->end_addr)
        return false;
    // 内部 Flash 是存储器映射：直接 memcpy
    memcpy(data, (const void *)addr, len);
    return true;
}

// G4 只能按 64bit(double-word) 对齐编程：要求 addr 8 字节对齐，
// 尾部不足 8 字节处补 0xFF 后整 double-word 写入（目标须已擦除）。
static bool fla_mcu_write(void *handle, uint32_t addr, const uint8_t *data, uint32_t len)
{
    const tFlaMcu *inst = (const tFlaMcu *)handle;
    if (!inst || !data || len == 0U)
        return false;
    if (addr < inst->start_addr || (addr + len - 1U) > inst->end_addr)
        return false;
    if ((addr & 7U) != 0U) // 硬件限制：double-word 对齐
        return false;

    if (HAL_FLASH_Unlock() != HAL_OK)
        return false;

    uint32_t i = 0U;
    bool ok = true;

    // 主体：完整 8 字节
    for (; ok && (i + 7U) < len; i += 8U)
    {
        uint64_t dword = 0U;
        for (uint32_t b = 0U; b < 8U; b++)
            dword |= (uint64_t)data[i + b] << (8U * b);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + i, dword) != HAL_OK)
            ok = false;
    }

    // 尾部：不足 8 字节，补 0xFF（= 擦除态）后整 double-word 写入
    if (ok && i < len)
    {
        uint64_t dword = 0xFFFFFFFFFFFFFFFFULL;
        for (uint32_t b = 0U; (i + b) < len; b++)
        {
            dword &= ~((uint64_t)0xFFU << (8U * b));
            dword |= (uint64_t)data[i + b] << (8U * b);
        }
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + i, dword) != HAL_OK)
            ok = false;
    }

    HAL_FLASH_Lock();
    return ok;
}

static bool fla_mcu_erase_sector(void *handle, uint8_t sec_id)
{
    (void)handle;
    if (sec_id >= MCU_NUM_PAGE_USR)
        return false;

    if (HAL_FLASH_Unlock() != HAL_OK)
        return false;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    bool ok = flash_erase_pages(USR_FIRST_PAGE + sec_id, 1U);

    if ((primask & 1U) == 0U)
        __enable_irq(); // 失败也必须恢复中断，否则中断被永久关闭
    HAL_FLASH_Lock();
    return ok;
}

static bool fla_mcu_erase_addr(void *handle, uint32_t addr, uint32_t len)
{
    const tFlaMcu *inst = (const tFlaMcu *)handle;
    if (!inst || len == 0U)
        return false;

    int first = page_index_at(addr);
    int last = page_index_at(addr + len - 1U);
    if (first < 0 || last < 0)
        return false;

    if (HAL_FLASH_Unlock() != HAL_OK)
        return false;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    bool ok = flash_erase_pages((uint32_t)first, (uint32_t)(last - first + 1));

    if ((primask & 1U) == 0U)
        __enable_irq();
    HAL_FLASH_Lock();
    return ok;
}

static uint8_t fla_mcu_sector_count(void *handle)
{
    (void)handle;
    return MCU_NUM_PAGE_USR;
}

static uint32_t fla_mcu_sector_addr(void *handle, uint8_t sec_id)
{
    (void)handle;
    if (sec_id >= MCU_NUM_PAGE_USR)
        return 0U;
    return PAGE_ADDR(USR_FIRST_PAGE + sec_id);
}

static uint32_t fla_mcu_sector_size(void *handle, uint8_t sec_id)
{
    (void)handle;
    if (sec_id >= MCU_NUM_PAGE_USR)
        return 0U;
    return FLASH_PAGE_BYTES;
}

// ---------- IAP：分区表 + 平台跳转 ----------

const tIAPPartition g_bl_parts = {
    .base = BL_START_ADDR,
    .size = BL_SIZE,
};
const tIAPPartition g_app_parts = {
    .base = APP_START_ADDR,
    .size = APP_SIZE,
};

// MCU App 初始化：复位向量表偏移至 BL 之后，承接bl跳转app 中断必须恢复（重启后）
void mcu_iap_app_init(void)
{
    SCB->VTOR = APP_START_ADDR;
    __enable_irq();
}

// 平台跳转：BL → 软复位；APP → 切向量表后跳转
bool mcu_iap_jump_bl(void)
{
    NVIC_SystemReset();
    return true;
}
bool mcu_iap_jump_app(void)
{

    // 校验目标向量表位于内部 flash 区（粗略防护）
    if ((APP_START_ADDR & 0xFFF00000U) != 0x08000000U)
        return false;

    __disable_irq();
    SCB->VTOR = APP_START_ADDR;
    __set_MSP(*(volatile uint32_t *)APP_START_ADDR);                  // 目标固件栈顶
    uint32_t reset_vec = *(volatile uint32_t *)(APP_START_ADDR + 4U); // 复位向量
    void (*jump)(void) = (void (*)(void))reset_vec;
    jump();
    return true; // 不可达
}

// ---- 驱动出口 ----
const tFlashOps fla_mcu_ops = {
    .open = fla_mcu_open,
    .read = fla_mcu_read,
    .write = fla_mcu_write,
    .erase_addr = fla_mcu_erase_addr,
    .erase_sector = fla_mcu_erase_sector,
    .sector_count = fla_mcu_sector_count,
    .sector_addr = fla_mcu_sector_addr,
    .sector_size = fla_mcu_sector_size,
};
