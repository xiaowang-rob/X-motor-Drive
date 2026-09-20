// ============================================================
// fla_mcu.c — 内部 MCU Flash 介质驱动（板级，直连 HAL）
//
// STM32F405，1MB：
//   - read  ：直接内存读（存储器映射）
//   - write ：按 word(4B) 编程，前导/尾部不足处按 byte；不做对齐假设
//   - erase ：按 F405 真实扇区几何（16K×4 + 64K×1 + 128K×7）逐扇区擦除
// 厂商库符号（HAL_FLASH_*、FLASH 寄存器）只出现在本文件。
// 实现 abs/flash.h 的 tFlashOps；IAP 出口见 board_flash.h。
// ============================================================
#include "fla_mcu.h"

#include "stm32f4xx_hal.h" // FLASH 编程接口（HAL 总头按 conf 决定是否展开）

#define FLASH_START_ADDR 0x08000000U   // Flash 起始地址
#define FLASH_CAPACITY (1024U * 1024U) // Flash 容量 (bytes)

#define MCU_FLASH_NUM_SECTORS 12U
#define MCU_NUM_SECTOR_BL 2U  // Bootloader 扇区数量
#define MCU_NUM_SECTOR_APP 7U // App 扇区数量
#define MCU_NUM_SECTOR_USR 3U // 用户数据扇区（1 参数 2 log 3 iap标志）

// ---------- 扇区几何（F405 真实布局） ----------
static const uint32_t SECTOR_BOUNDS[MCU_FLASH_NUM_SECTORS + 1] = {
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
    0x08100000U, // 结束地址，方便计算
};

static const uint8_t BL_SECTOR_ID[MCU_NUM_SECTOR_BL] = {0U, 1U};
static const uint8_t APP_SECTOR_ID[MCU_NUM_SECTOR_APP] = {2U, 3U, 4U, 5U, 6U, 7U, 8U};
static const uint8_t USR_SECTOR_ID[MCU_NUM_SECTOR_USR] = {9U, 10U, 11U};

// 分区几何（编译期常量）
#define BL_START_ADDR (SECTOR_BOUNDS[BL_SECTOR_ID[0]])
#define APP_START_ADDR (SECTOR_BOUNDS[APP_SECTOR_ID[0]])
#define USR_START_ADDR (SECTOR_BOUNDS[USR_SECTOR_ID[0]])
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

// ---------- 地址 → 扇区索引（0..11）；越界返回 -1 ----------
static int sector_index_at(const tFlaMcu *inst, uint32_t addr)
{
    if (addr < inst->start_addr || addr > inst->end_addr)
        return -1;
    uint8_t i = 0U;
    for (; i < MCU_FLASH_NUM_SECTORS; i++)
    {
        if (addr < SECTOR_BOUNDS[i])
            break;
    }
    return (int)i - 1;
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
    // 内部 Flash 是存储器映射：直接 memcpy（按字拷贝，远快于逐字节 volatile 读）
    memcpy(data, (const void *)addr, len);
    return true;
}

// 以 word 编程；前导/尾部不足 4 字节处用 byte 编程补齐。
// 注：原实现无条件按 word 写，addr 未 4 字节对齐时 HAL 会拒绝 —— 这里修正。
static bool fla_mcu_write(void *handle, uint32_t addr, const uint8_t *data, uint32_t len)
{
    const tFlaMcu *inst = (const tFlaMcu *)handle;
    if (!inst || !data || len == 0U)
        return false;
    if (addr < inst->start_addr || (addr + len - 1U) > inst->end_addr)
        return false;

    if (HAL_FLASH_Unlock() != HAL_OK)
        return false;

    uint32_t i = 0U;
    bool ok = true;

    // 前导：补齐到 4 字节对齐
    for (; i < len && ((addr + i) & 3U) != 0U; i++)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, addr + i, data[i]) != HAL_OK)
        {
            ok = false;
            break;
        }
    }

    // 主体：按 word
    for (; ok && (i + 3U) < len; i += 4U)
    {
        uint32_t word = (uint32_t)data[i] | ((uint32_t)data[i + 1U] << 8) |
                        ((uint32_t)data[i + 2U] << 16) | ((uint32_t)data[i + 3U] << 24);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word) != HAL_OK)
        {
            ok = false;
            break;
        }
    }

    // 尾部：不足 4 字节
    for (; ok && i < len; i++)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, addr + i, data[i]) != HAL_OK)
        {
            ok = false;
            break;
        }
    }

    HAL_FLASH_Lock();
    return ok;
}

static bool fla_mcu_erase_sector(void *handle, uint8_t sec_id)
{
    (void)handle;
    if (sec_id >= MCU_NUM_SECTOR_USR)
        return false;

    if (HAL_FLASH_Unlock() != HAL_OK)
        return false;

    uint32_t s = USR_SECTOR_ID[sec_id];
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sector_err = 0U;
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = s;
    erase.NbSectors = 1U;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    bool ok = (HAL_FLASHEx_Erase(&erase, &sector_err) == HAL_OK);

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

    int first = sector_index_at(inst, addr);
    int last = sector_index_at(inst, addr + len - 1U);
    if (first < 0 || last < 0)
        return false;

    if (HAL_FLASH_Unlock() != HAL_OK)
        return false;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sector_err = 0U;
    bool ok = true;
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

    if ((primask & 1U) == 0U)
        __enable_irq();
    HAL_FLASH_Lock();
    return ok;
}

static uint8_t fla_mcu_sector_count(void *handle)
{
    (void)handle;
    return MCU_NUM_SECTOR_USR;
}

static uint32_t fla_mcu_sector_addr(void *handle, uint8_t sec_id)
{
    (void)handle;
    if (sec_id >= MCU_NUM_SECTOR_USR)
        return 0U;
    return SECTOR_BOUNDS[USR_SECTOR_ID[sec_id]];
}

static uint32_t fla_mcu_sector_size(void *handle, uint8_t sec_id)
{
    (void)handle;
    if (sec_id >= MCU_NUM_SECTOR_USR)
        return 0U;
    return SECTOR_BOUNDS[USR_SECTOR_ID[sec_id] + 1U] - SECTOR_BOUNDS[USR_SECTOR_ID[sec_id]];
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

// MCU App 初始化：复位向量表偏移至 BL 之后，中断必须恢复（重启后）
void mcu_iap_app_init(void)
{
    SCB->VTOR = FLASH_START_ADDR | BL_SIZE;
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
