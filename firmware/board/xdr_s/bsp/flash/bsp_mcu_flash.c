// ============================================================
// 内部 MCU Flash 介质驱动
//
// STM32G474CB，128KB Flash（32 页 × 4KB，单Bank）：
//   - read  ：直接内存读（存储器映射）
//   - write ：按 double-word(64bit) 编程（G4 只能 64bit 对齐编程，
//             F4 的 byte/word 编程在 G4 上不存在）
//   - erase ：按页擦除（FLASH_TYPEERASE_PAGES），并按 bank 边界自动拆分
//             （G474 若 DBANK=1 则页号在 bank 内重新计数）
// ============================================================
#include "bsp_flash.h"

#include "stm32g4xx_hal.h" // FLASH 编程接口（HAL 总头按 conf 决定是否展开）

#define FLASH_START_ADDR 0x08000000U  // Flash 起始地址
#define FLASH_CAPACITY (128U * 1024U) // Flash 容量 (bytes)
#define FLASH_PAGE_BYTES                                                       \
    4096U // 页大小（双Bank G4：2KB）页大小（单Bank G4：4KB）
#define FLASH_TOTAL_PAGES (FLASH_CAPACITY / FLASH_PAGE_BYTES) // 32页

#define MCU_NUM_PAGE_BL 0U    // 32 KiB Bootloader
#define MCU_NUM_PAGE_APP 28U  // 80 KiB App
#define MCU_NUM_PAGE_PARAM 3U // 12 KiB 参数数据
#define MCU_NUM_PAGE_LOG 1U   // 4 KiB 日志数据

#define BL_FIRST_PAGE 0U
#define APP_FIRST_PAGE (BL_FIRST_PAGE + MCU_NUM_PAGE_BL)
#define PARAM_FIRST_PAGE (APP_FIRST_PAGE + MCU_NUM_PAGE_APP)
#define LOG_FIRST_PAGE (PARAM_FIRST_PAGE + MCU_NUM_PAGE_PARAM)

#define PAGE_ADDR(p) (FLASH_START_ADDR + (p) * FLASH_PAGE_BYTES)

// 分区几何（编译期常量）：BL → APP → PARAM → LOG
#define BL_START_ADDR PAGE_ADDR(BL_FIRST_PAGE)
#define APP_START_ADDR PAGE_ADDR(APP_FIRST_PAGE)
#define PARAM_START_ADDR PAGE_ADDR(PARAM_FIRST_PAGE)
#define LOG_START_ADDR PAGE_ADDR(LOG_FIRST_PAGE)

#define BL_SIZE (APP_START_ADDR - BL_START_ADDR)
#define APP_SIZE (PARAM_START_ADDR - APP_START_ADDR)
#define PARAM_SIZE (LOG_START_ADDR - PARAM_START_ADDR)
#define LOG_SIZE (FLASH_START_ADDR + FLASH_CAPACITY - LOG_START_ADDR)

// ---------- 实例 ----------
typedef struct tMcuFlashPart {
    uint32_t start_addr; // 分区起始地址
    uint32_t size;
} tMcuFlashPart;

tMcuFlashPart param_part = {
    .start_addr = PARAM_START_ADDR,
    .size = PARAM_SIZE,
};
tMcuFlashPart log_part = {
    .start_addr = LOG_START_ADDR,
    .size = LOG_SIZE,
};
tMcuFlashPart iap_part = {
    .start_addr = APP_START_ADDR,
    .size = APP_SIZE,
};

// ---------- bank/页换算（双 bank 器件页号在 bank 内重新计数） ----------
static uint32_t pages_per_bank(void) {
#if defined(FLASH_OPTR_DBANK)
    if (READ_BIT(FLASH->OPTR, FLASH_OPTR_DBANK) != 0U)
        return FLASH_TOTAL_PAGES / 2U;
#endif
    return FLASH_TOTAL_PAGES;
}

static void page_to_bank_page(uint32_t page, uint32_t *bank,
                              uint32_t *bank_page) {
#if defined(FLASH_OPTR_DBANK)
    if (READ_BIT(FLASH->OPTR, FLASH_OPTR_DBANK) != 0U) {
        uint32_t per_bank = FLASH_TOTAL_PAGES / 2U;
        if (page < per_bank) {
            *bank = FLASH_BANK_1;
            *bank_page = page;
        } else {
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
static int page_index_at(uint32_t addr) {
    if (addr < FLASH_START_ADDR ||
        addr > (FLASH_START_ADDR + FLASH_CAPACITY - 1U))
        return -1;
    return (int)((addr - FLASH_START_ADDR) / FLASH_PAGE_BYTES);
}

// 擦除连续的 n 页（起始页 first_page）；按 bank 边界自动拆分
static bool flash_erase_pages(uint32_t first_page, uint32_t n) {
    uint32_t per_bank = pages_per_bank();

    while (n > 0U) {
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

static bool fla_mcu_open(void *handle, uint32_t *size) {
    tMcuFlashPart *inst = (tMcuFlashPart *)handle;
    if (!inst || !size)
        return false;

    *size = inst->size;
    return true; // 内部 Flash 无需初始化
}

static bool fla_mcu_read(void *handle, uint32_t addr, uint8_t *data,
                         uint32_t len) {
    const tMcuFlashPart *inst = (const tMcuFlashPart *)handle;
    if (!inst || !data || len == 0U)
        return false;
    uint32_t flash_addr = addr + inst->start_addr;
    if (flash_addr < inst->start_addr || len > inst->size)
        return false;

    // 内部 Flash 是存储器映射：直接 memcpy
    memcpy(data, (const void *)flash_addr, len);
    return true;
}

static bool fla_mcu_write(void *handle, uint32_t addr, const uint8_t *data,
                          uint32_t len) {
    const tMcuFlashPart *inst = (const tMcuFlashPart *)handle;

    if (!inst || !data || len == 0U)
        return false;

    /* 分区起始地址必须 Double Word 对齐 */
    if ((inst->start_addr & 7U) != 0U)
        return false;

    /* 起始地址必须 Double Word 对齐 */
    if ((addr & 7U) != 0U)
        return false;

    /* 检查逻辑写入范围 */
    if (addr > inst->size || len > inst->size - addr)
        return false;

    /* 实际编程长度向上对齐到 Double Word */
    uint32_t aligned_len = (len + 7U) & ~7U;

    /* 检查实际编程范围，防止尾部填充越过分区 */
    if (aligned_len > inst->size - addr)
        return false;

    uint32_t flash_addr = inst->start_addr + addr;

    if (HAL_FLASH_Unlock() != HAL_OK)
        return false;

    bool ok = true;

    for (uint32_t i = 0U; i < aligned_len; i += 8U) {
        /* 默认填充 0xEE */
        uint8_t buf[8];
        memset(buf, 0xEE, sizeof(buf));

        /* 当前 Double Word 中实际数据长度 */
        uint32_t copy_len = 0U;

        if (i < len) {
            copy_len = len - i;

            if (copy_len > sizeof(buf))
                copy_len = sizeof(buf);

            memcpy(buf, &data[i], copy_len);
        }

        uint64_t dword = 0U;
        memcpy(&dword, buf, sizeof(dword));

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                              flash_addr + i,
                              dword) != HAL_OK) {
            ok = false;
            break;
        }
    }

    HAL_FLASH_Lock();

    return ok;
}

static bool fla_mcu_erase_addr(void *handle, uint32_t addr, uint32_t len) {
    const tMcuFlashPart *inst = (const tMcuFlashPart *)handle;
    if (!inst || len == 0U)
        return false;
    uint32_t flash_addr = addr + inst->start_addr;
    int first = page_index_at(flash_addr);
    int last = page_index_at(flash_addr + len - 1U);
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

// MCU App 初始化：复位向量表偏移至 BL 之后，承接bl跳转app
// 中断必须恢复（重启后）
void mcu_iap_app_init(void) {
    SCB->VTOR = APP_START_ADDR;
    __enable_irq();
}

// ---- 驱动出口 ----
const tStorageOps mcu_fla_ops = {
    .open = fla_mcu_open,
    .read = fla_mcu_read,
    .write = fla_mcu_write,
    .erase_addr = fla_mcu_erase_addr,
};

static const tStorageDevHandle param_part_ha = {
    .ops = &mcu_fla_ops,
    .dev = &param_part,
};
static const tStorageDevHandle log_part_ha = {
    .ops = &mcu_fla_ops,
    .dev = &log_part,
};
static const tStorageDevHandle iap_part_ha = {
    .ops = &mcu_fla_ops,
    .dev = &iap_part,
};
const tStorageDevHandle *Get_ParamPartHandle(void) { return &param_part_ha; }
const tStorageDevHandle *Get_LogPartHandle(void) { return &log_part_ha; }
const tStorageDevHandle *Get_IapPartHandle(void) { return &iap_part_ha; }