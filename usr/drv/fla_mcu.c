// ============================================================
// mcu_flash_drv.c — 内部 MCU Flash 介质驱动（xdr_p_o1.2/STM32F405，v2 直连版）
//
// 实现 tFlashDriverOps：
//   - read：直接内存读
//   - write：半字(16bit)编程，要求 addr 2 字节对齐；尾部奇数字节补 0xFF 写入
//   - erase：按 F405 真实扇区几何（16K×4 + 64K×1 + 128K×7）逐扇区擦除
// 厂商库符号（HAL_FLASH_*、FLASH 寄存器）只出现在本文件。
// ============================================================

#include "flash_drivers.h"

#include "platform.h"
// 显式引入 FLASH 编程接口（HAL 总头按 conf 决定是否展开）
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_flash_ex.h"

typedef struct
{
    eDeviceStatus dstate;
} tMcuFlash_ctx;

// 地址 → 扇区索引（0..11）；越界返回 -1
static int sector_index_at(uint32_t addr)
{
    if (addr < FLASH_START_ADDR || addr >= FLASH_END_ADDR)
        return -1;
    uint8_t i = 0U;
    for (; i < MCU_FLASH_NUM_SECTORS; i++)
    {
        if (addr < SECTOR_BOUNDS[i])
            break;
    }
    return i - 1;
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
    if (addr < FLASH_START_ADDR || (addr + len) > FLASH_END_ADDR)
        return false;
    for (uint32_t i = 0U; i < len; i++)
        data[i] = *(volatile uint8_t *)(addr + i);
    return true;
}

// 以word写入 会自动补齐
static bool mf_write(FlashChipHandle h, uint32_t addr, const uint8_t *data, uint32_t len)
{
    (void)h;
    if (addr < FLASH_START_ADDR || (addr + len) > FLASH_END_ADDR)
        return false;
    HAL_FLASH_Unlock();
    uint16_t i = 0;

    // 先按字（4字节）写入
    for (; i + 3 < len; i += 4)
    {
        uint32_t word = data[i] | (data[i + 1] << 8) | (data[i + 2] << 16) | (data[i + 3] << 24);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return false;
        }
    }

    // 处理剩余不足4字节（按字节写入，但注意 STM32F4 的字节编程要求半字对齐？实际支持字节，但效率低）
    while (i < len)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, addr + i, data[i]) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return false;
        }
        i++;
    }

    HAL_FLASH_Lock();
    return true;
}
static bool mf_erase_sector(FlashChipHandle h, uint8_t sec_id)
{
    (void)h;
    if (sec_id < 0 || sec_id >= MCU_FLASH_NUM_SECTORS)
        return false;

    if (HAL_FLASH_Unlock() != HAL_OK)
        return false;

    platform_disable_irq();
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sector_err = 0U;
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = (uint32_t)s;
    erase.NbSectors = 1U;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    if (HAL_FLASHEx_Erase(&erase, &sector_err) != HAL_OK)
    {
        return false;
    }
    platform_enable_irq();

    HAL_FLASH_Lock();

    return true;
}
// 按地址擦除
static bool mf_erase_addr(FlashChipHandle h, uint32_t addr, uint32_t len)
{
    (void)h;
    if (len == 0U)
        return false;

    int first = sector_index_at(addr);
    int last = sector_index_at(addr + len - 1U);
    if (first < 0 || last < 0)
        return false;

    if (HAL_FLASH_Unlock() != HAL_OK)
        return false;

    platform_disable_irq();

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
            platform_enable_irq();

            HAL_FLASH_Lock();
            return false;
        }
    }
    platform_enable_irq();

    HAL_FLASH_Lock();

    return true;
}
static uint32_t mf_get_bl_ids(FlashChipHandle h, uint8_t *num)
{
    (void)h;
    *num = MCU_NUM_SECTOR_BL;
    return BL_SECTOR_ID;
}
static uint32_t mf_get_app_ids(FlashChipHandle h, uint8_t *num)
{
    (void)h;
    *num = MCU_NUM_SECTOR_APP;
    return APP_SECTOR_ID;
}
static uint32_t mf_get_usr_ids(FlashChipHandle h, uint8_t *num)
{
    (void)h;
    *num = MCU_NUM_SECTOR_USR;
    return USR_SECTOR_ID;
}

static uint32_t mf_get_sec_addr(FlashChipHandle h, uint8_t sec_id)
{
    (void)h;
    return SECTOR_BOUNDS[sec_id];
}
static uint32_t mf_get_sec_size(FlashChipHandle h, uint8_t sec_id)
{
    (void)h;
    return SECTOR_BOUNDS[sec_id + 1U] - SECTOR_BOUNDS[sec_id];
}
static bool mf_jump_to_app(FlashChipHandle h)
{
    if (!h)
        return false;
    plat_jump_to_app();
    return true;
}
static bool mf_jump_to_bl(FlashChipHandle h)
{
    if (!h)
        return false;
    plat_system_reset();
    return true;
}
static uint8_t mf_get_state(FlashChipHandle h)
{
    tMcuFlash_ctx *ctx = (tMcuFlash_ctx *)h;
    return (uint8_t)(ctx ? ctx->dstate : DEV_OFFLINE);
}

const tFlashDriverOps mcu_flash_driver_ops = {
    .init = mf_init,
    .read = mf_read,
    .write = mf_write,
    .erase_addr = mf_erase_addr,
    .erase_sector = mf_erase_sector,
    .get_bl_ids = mf_get_bl_ids,
    .get_app_ids = mf_get_app_ids,
    .get_usr_ids = mf_get_usr_ids,
    .get_sector_addr = mf_get_sec_addr,
    .get_sector_size = mf_get_sec_size,
    .jump_app = mf_jump_to_app,
    .jump_bl = mf_jump_to_bl,
    .get_state = mf_get_state,
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
    h = NULL;
}
