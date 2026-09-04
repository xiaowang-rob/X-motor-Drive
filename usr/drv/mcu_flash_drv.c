// ============================================================
// mcu_flash_drv.c — 内部 MCU Flash 介质驱动（xdr_p_o1.2/STM32F405，v2 直连版）
//
// 实现 tFlashDriverOps：
//   - read：直接内存读
//   - write：半字(16bit)编程，要求 addr 2 字节对齐；尾部奇数字节补 0xFF 写入
//   - erase：按 F405 真实扇区几何（16K×4 + 64K×1 + 128K×7）逐扇区擦除
// 厂商库符号（HAL_FLASH_*、FLASH 寄存器）只出现在本文件。
// ============================================================

#include <stdlib.h>

#include "usr/abs/device.h"
#include "usr/abs/flash.h"

#include "platform.h" // 中断开关（擦/写临界）
#include "mcu_flash_drv.h"

// 显式引入 FLASH 编程接口（HAL 总头按 conf 决定是否展开）
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_flash_ex.h"

#define MCU_FLASH_BASE 0x08000000U
#define MCU_FLASH_CAPACITY (1024U * 1024U)
#define MCU_FLASH_NUM_SECTORS 12U
#define MCU_FLASH_PROG_UNIT 2U // half-word

// F405 1MB 扇区边界（扇区 0..11 起始地址）
static const uint32_t SECTOR_BOUNDS[MCU_FLASH_NUM_SECTORS] = {
    0x08000000U, 0x08004000U, 0x08008000U, 0x0800C000U, // 0-3: 16KB
    0x08010000U,                                        // 4: 64KB
    0x08020000U, 0x08040000U, 0x08060000U,              // 5-7: 128KB
    0x08080000U, 0x080A0000U, 0x080C0000U, 0x080E0000U, // 8-11: 128KB
};

typedef struct
{
    eDeviceStatus dstate;
} tMcuFlash_ctx;

// 地址 → 扇区索引（0..11）；越界返回 -1
static int sector_index_at(uint32_t addr)
{
    if (addr < SECTOR_BOUNDS[0] || addr >= SECTOR_BOUNDS[MCU_FLASH_NUM_SECTORS - 1U] + 0x20000U)
        return -1;
    for (uint8_t i = 0U; i < MCU_FLASH_NUM_SECTORS; i++)
    {
        uint32_t end = (i + 1U < MCU_FLASH_NUM_SECTORS) ? SECTOR_BOUNDS[i + 1U]
                                                        : SECTOR_BOUNDS[i] + 0x20000U;
        if (addr >= SECTOR_BOUNDS[i] && addr < end)
            return (int)i;
    }
    return -1;
}

// ---- ops 实现 ----

static bool mf_init(FlashChipHandle h)
{
    tMcuFlash_ctx *ctx = (tMcuFlash_ctx *)h;
    if (!ctx)
        return false;
    ctx->dstate = DEV_ONLINE;
    return true;
}

static bool mf_read(FlashChipHandle h, uint32_t addr, uint8_t *data, uint32_t len)
{
    (void)h;
    if (addr < MCU_FLASH_BASE || (addr + len) > MCU_FLASH_BASE + MCU_FLASH_CAPACITY)
        return false;
    for (uint32_t i = 0U; i < len; i++)
        data[i] = *(volatile uint8_t *)(addr + i);
    return true;
}

static bool mf_write(FlashChipHandle h, uint32_t addr, const uint8_t *data, uint32_t len)
{
    tMcuFlash_ctx *ctx = (tMcuFlash_ctx *)h;
    if (!ctx || (addr & 1U) != 0U) // 半字对齐要求
        return false;
    if (addr < MCU_FLASH_BASE || (addr + len) > MCU_FLASH_BASE + MCU_FLASH_CAPACITY)
        return false;

    if (HAL_FLASH_Unlock() != HAL_OK)
        return false;

    platform_disable_irq(); // 编程期间停中断
    bool ok = true;
    uint32_t i = 0U;
    while (i < len)
    {
        uint16_t word;
        if (i + 1U < len)
            word = (uint16_t)(data[i] | ((uint16_t)data[i + 1U] << 8));
        else
            word = (uint16_t)(data[i] | 0xFF00U); // 尾部奇数补 FF（对已擦区无副作用）

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i, word) != HAL_OK)
        {
            ok = false;
            break;
        }
        i += MCU_FLASH_PROG_UNIT;
    }
    platform_enable_irq();

    HAL_FLASH_Lock();
    ctx->dstate = ok ? DEV_RUNNING : DEV_RUN_ERROR;
    return ok;
}

static bool mf_erase(FlashChipHandle h, uint32_t addr, uint32_t len)
{
    tMcuFlash_ctx *ctx = (tMcuFlash_ctx *)h;
    if (!ctx || len == 0U)
        return false;

    int first = sector_index_at(addr);
    int last = sector_index_at(addr + len - 1U);
    if (first < 0 || last < 0)
        return false;

    if (HAL_FLASH_Unlock() != HAL_OK)
        return false;

    platform_disable_irq();
    bool ok = true;
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sector_err = 0U;
    for (int s = first; s <= last; s++)
    {
        erase.TypeErase = FLASH_TYPEERASE_SECTORS;
        erase.Sector = (uint32_t)s;
        erase.NbSectors = 1U;
        erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
        if (HAL_FLASHEx_Erase(&erase, &sector_err) != HAL_OK)
        {
            ok = false;
            break;
        }
    }
    platform_enable_irq();

    HAL_FLASH_Lock();
    ctx->dstate = ok ? DEV_RUNNING : DEV_RUN_ERROR;
    return ok;
}

static uint32_t mf_capacity(FlashChipHandle h)
{
    (void)h;
    return MCU_FLASH_CAPACITY;
}

static uint32_t mf_page_size(FlashChipHandle h)
{
    (void)h;
    return MCU_FLASH_PROG_UNIT; // half-word 编程粒度
}

// 契约的单一擦除单元值：128KB（单元 attach 限制）；erase 内部按真实几何
static uint32_t mf_sector_size(FlashChipHandle h)
{
    (void)h;
    return 128U * 1024U;
}

static uint8_t mf_state(FlashChipHandle h)
{
    tMcuFlash_ctx *ctx = (tMcuFlash_ctx *)h;
    return (uint8_t)(ctx ? ctx->dstate : DEV_OFFLINE);
}

const tFlashDriverOps mcu_flash_driver_ops = {
    .init = mf_init,
    .read = mf_read,
    .write = mf_write,
    .erase = mf_erase,
    .get_capacity = mf_capacity,
    .get_page_size = mf_page_size,
    .get_sector_size = mf_sector_size,
    .get_state = mf_state,
};

FlashChipHandle mcu_flash_create(void)
{
    tMcuFlash_ctx *ctx = (tMcuFlash_ctx *)calloc(1U, sizeof(tMcuFlash_ctx));
    if (!ctx)
        return NULL;
    ctx->dstate = DEV_OFFLINE;
    return (FlashChipHandle)ctx;
}

void mcu_flash_destroy(FlashChipHandle h)
{
    free(h);
}
