// ============================================================
// fla_mcu.c — 内部 MCU Flash 介质驱动（板级，直连 HAL）
//
// STM32F405，1MB：
//   - read  ：直接内存读（存储器映射）
//   - write ：按 word(4B) 编程，前导/尾部不足处按 byte；不做对齐假设
//   - erase ：按 F405 真实扇区几何（16K×4 + 64K×1 + 128K×7）逐扇区擦除
// 厂商库符号（HAL_FLASH_*、FLASH 寄存器）只出现在本文件。
// 本文件实现 app/abs/flash_board.h 的 MCU 路由，并给出 IAP 配置。
// ============================================================
#include "fla_mcu.h"

#include "board_flash.h"

#include "stm32f4xx_hal.h" // FLASH 编程接口（HAL 总头按 conf 决定是否展开）

#define FLASH_START_ADDR 0x08000000U   // Flash 起始地址
#define FLASH_CAPACITY (1024U * 1024U) // Flash 容量 (bytes)
#define FLASH_END_ADDR 0x080FFFFFU     // Flash 结束地址

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

// ---------- 地址 → 扇区索引（0..11）；越界返回 -1 ----------
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
    return (int)i - 1;
}

// ---------- 板级钩子实现（MCU 路由） ----------

bool fla_mcu_open(void)
{
    return true; // 内部 Flash 无需初始化
}

bool fla_mcu_read(uint32_t addr, uint8_t *data, uint32_t len)
{
    if (!data || len == 0U)
        return false;
    if (addr < FLASH_START_ADDR || (addr + len) > FLASH_END_ADDR)
        return false;
    // 内部 Flash 是存储器映射：直接 memcpy（按字拷贝，远快于逐字节 volatile 读）
    memcpy(data, (const void *)addr, len);
    return true;
}

// 以 word 编程；前导/尾部不足 4 字节处用 byte 编程补齐。
// 注：原实现无条件按 word 写，addr 未 4 字节对齐时 HAL 会拒绝 —— 这里修正。
bool fla_mcu_write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    if (!data || len == 0U)
        return false;
    if (addr < FLASH_START_ADDR || (addr + len) > FLASH_END_ADDR)
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

bool fla_mcu_erase_sector(uint8_t sec_id)
{
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

bool fla_mcu_erase_addr(uint32_t addr, uint32_t len)
{
    if (len == 0U)
        return false;

    int first = sector_index_at(addr);
    int last = sector_index_at(addr + len - 1U);
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

uint8_t fla_mcu_sector_count(void)
{
    return MCU_NUM_SECTOR_USR;
}

uint32_t fla_mcu_sector_addr(uint8_t sec_id)
{
    if (sec_id >= MCU_NUM_SECTOR_USR)
        return 0U;
    return SECTOR_BOUNDS[USR_SECTOR_ID[sec_id]];
}

uint32_t fla_mcu_sector_size(uint8_t sec_id)
{
    if (sec_id >= MCU_NUM_SECTOR_USR)
        return 0U;
    return SECTOR_BOUNDS[USR_SECTOR_ID[sec_id] + 1U] - SECTOR_BOUNDS[USR_SECTOR_ID[sec_id]];
}

// ---------- IAP：分区表 + 平台跳转 ----------

// 分区表（[IAP_BL] / [IAP_APP]），几何来自上面的扇区计算
static tIAPPartition s_iap_parts[IAP_PART_COUNT];

static void iap_parts_init(void)
{
    s_iap_parts[IAP_BL].base = BL_START_ADDR;
    s_iap_parts[IAP_BL].size = BL_SIZE;
    s_iap_parts[IAP_APP].base = APP_START_ADDR;
    s_iap_parts[IAP_APP].size = APP_SIZE;
}

// MCU App 初始化：复位向量表偏移至 BL 之后，中断必须恢复（重启后）
void mcu_app_init(void)
{
    SCB->VTOR = FLASH_START_ADDR | BL_SIZE;
    __enable_irq();
}

// 平台跳转：BL → 软复位；APP → 切向量表后跳转
static bool mcu_iap_jump(eIAPtype type)
{
    if (type == IAP_BL)
    {
        NVIC_SystemReset();
        return true;
    }

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

// 取 IAP 配置（组装层据此初始化 tIAP）：介质为 MCU 内部 Flash，
// 分区表与平台跳转由本文件提供；整区校验由 abs 的 iap_verify_crc 完成。
tIAPConfig mcu_iap_config(eIAPtype type)
{
    iap_parts_init();

    tIAPConfig cfg = {
        .flash_dev = FLASH_DEV_MCU,
        .parts = s_iap_parts,
        .part_count = IAP_PART_COUNT,
        .jump = mcu_iap_jump,
        .type = type,
    };
    return cfg;
}
