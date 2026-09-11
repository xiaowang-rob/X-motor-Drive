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

// 显式引入 FLASH 编程接口（HAL 总头按 conf 决定是否展开）
#include "stm32f4xx_hal_flash.h"

#define FLASH_START_ADDR 0x08000000U   // Flash 起始地址
#define FLASH_CAPACITY (1024U * 1024U) // Flash 容量 (bytes)
#define FLASH_END_ADDR 0x080FFFFFU     // Flash 结束地址
#define NORMAL_MAGIC 0xFFFFFFFF        // 空数

#define MCU_FLASH_NUM_SECTORS 12U
#define MCU_NUM_SECTOR_BL 2U  // Bootloader 扇区数量
#define MCU_NUM_SECTOR_APP 7U // App 扇区数量
#define MCU_NUM_SECTOR_USR 3U // 用户数据扇区数量-至少三个，1 参数 2 log 3 iap标志

const uint32_t SECTOR_BOUNDS[MCU_FLASH_NUM_SECTORS + 1];

const uint8_t BL_SECTOR_ID[MCU_NUM_SECTOR_BL];
const uint8_t APP_SECTOR_ID[MCU_NUM_SECTOR_APP];
const uint8_t USR_SECTOR_ID[MCU_NUM_SECTOR_USR];

const uint32_t BL_START_ADDR;
const uint32_t BL_SIZE;
const uint32_t APP_START_ADDR;
const uint32_t APP_SIZE;
const uint32_t USR_START_ADDR;
const uint32_t USR_SIZE;

const uint32_t SECTOR_BOUNDS[MCU_FLASH_NUM_SECTORS + 1] = {
    0x08000000U,
    0x08004000U,
    0x08008000U,
    0x0800C000U, // 0-3: 16KB
    0x08010000U, // 4: 64KB
    0x08020000U,
    0x08040000U,
    0x08060000U, // 5-7: 128KB
    0x08080000U,
    0x080A0000U,
    0x080C0000U,
    0x080E0000U, // 8-11: 128KB
    0x08100000U, // 结束地址 方便计算
};

const uint8_t BL_SECTOR_ID[MCU_NUM_SECTOR_BL] = {0U, 1U};

const uint8_t APP_SECTOR_ID[MCU_NUM_SECTOR_APP] = {2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U};
const uint8_t USR_SECTOR_ID[MCU_NUM_SECTOR_USR] = {10U, 11U, 12U};

const uint32_t BL_START_ADDR = SECTOR_BOUNDS[BL_SECTOR_ID[0]];
const uint32_t APP_START_ADDR = SECTOR_BOUNDS[APP_SECTOR_ID[0]];

const uint32_t USR_START_ADDR = SECTOR_BOUNDS[USR_SECTOR_ID[0]];

const uint32_t BL_SIZE = APP_START_ADDR - BL_START_ADDR;
const uint32_t APP_SIZE = USR_START_ADDR - APP_START_ADDR;
const uint32_t USR_SIZE = SECTOR_BOUNDS[MCU_FLASH_NUM_SECTORS] - USR_START_ADDR;

// ---------- 启动配置 ----------
#define VECT_TABLE_OFFSET BL_SIZE // 中断向量表偏移 整个BL的空间

// ---------- flash 驱动 ----------

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
static bool mf_erase_usr_sector(FlashChipHandle h, uint8_t sec_id)
{
    (void)h;
    if (sec_id < 0 || sec_id >= MCU_NUM_SECTOR_USR)
        return false;

    if (HAL_FLASH_Unlock() != HAL_OK)
        return false;

    uint32_t s = USR_SECTOR_ID[sec_id];
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
static uint8_t mf_get_usr_sector_count(FlashChipHandle h)
{
    (void)h;
    return MCU_NUM_SECTOR_USR;
}

static uint32_t mf_get_usr_sec_addr(FlashChipHandle h, uint8_t sec_id)
{
    (void)h;
    return SECTOR_BOUNDS[USR_SECTOR_ID[sec_id]];
}
static uint32_t mf_get_usr_sec_size(FlashChipHandle h, uint8_t sec_id)
{
    (void)h;
    return SECTOR_BOUNDS[USR_SECTOR_ID[sec_id] + 1U] - SECTOR_BOUNDS[USR_SECTOR_ID[sec_id]];
}

static uint8_t mf_get_state(FlashChipHandle h)
{
    (void)h;
    return DEV_ONLINE;
}

const tFlashDriverOps mcu_flash_driver_ops = {
    .init = NULL, // MCU Flash 无初始化
    .read = mf_read,
    .write = mf_write,
    .erase_addr = mf_erase_addr,
    .erase_sector = mf_erase_usr_sector,
    .get_sector_count = mf_get_usr_sector_count,
    .get_sector_addr = mf_get_usr_sec_addr,
    .get_sector_size = mf_get_usr_sec_size,

    .get_state = mf_get_state,
};

// ---------- IAP 驱动 ----------

static void mf_app_init(void)
{
    SCB->VTOR = VECT_TABLE_OFFSET;
}
static bool mf_erase_app(void)
{
    return mf_erase_addr(void, APP_START_ADDR, APP_SIZE);
}
static bool mf_write_app(uint32_t offset, const uint8_t *data, uint32_t len)
{
    uint32_t addr = offset + APP_START_ADDR;
    return mf_write(void, addr, data, len);
}
static bool mf_read_app(uint32_t offset, uint8_t *data, uint32_t len)
{
    uint32_t addr = offset + APP_START_ADDR;
    return mf_read(void, addr, data, len);
}
static bool mf_erase_bl(void)
{
    return mf_erase_addr(void, BL_START_ADDR, BL_SIZE);
}
static bool mf_write_bl(uint32_t offset, const uint8_t *data, uint32_t len)
{
    uint32_t addr = offset + BL_START_ADDR;
    return mf_write(void, addr, data, len);
}
static bool mf_read_bl(uint32_t offset, uint8_t *data, uint32_t len)
{
    uint32_t addr = offset + BL_START_ADDR;
    return mf_read(void, addr, data, len);
}
static bool mf_jump_to_app(void)
{
    // 校验向量表位于内部 flash 区（粗略防护），随后关中断并跳转
    if ((APP_START_ADDR & 0xFFF00000U) != 0x08000000U)
        return;

    plat_disable_irq();
    SCB->VTOR = APP_START_ADDR;
    __set_MSP(*(volatile uint32_t *)APP_START_ADDR); // 目标固件的栈顶
    typedef void (*p_fn)(void);
    p_fn jump = (p_fn)(*(volatile uint32_t *)(APP_START_ADDR + 4U)); // 复位向量
    jump();
    return true;
}
static bool mf_jump_to_bl(void)
{
    NVIC_SystemReset();
    return true;
}

const tIAPDriverOps mcu_iap_driver_ops = {
    .app_init = mf_app_init,
    .app_erase = mf_erase_app,
    .app_write = mf_write_app,
    .app_read = mf_read_app,
    .bl_erase = mf_erase_bl,
    .bl_write = mf_write_bl,
    .bl_read = mf_read_bl,
    .jump_app = mf_jump_to_app,
    .jump_bl = mf_jump_to_bl,
};
