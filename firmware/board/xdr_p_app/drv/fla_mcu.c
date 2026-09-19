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
#include "IF_irq.h"
// 显式引入 FLASH 编程接口（HAL 总头按 conf 决定是否展开）
#include "stm32f4xx_hal.h"

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

const uint8_t APP_SECTOR_ID[MCU_NUM_SECTOR_APP] = {2U, 3U, 4U, 5U, 6U, 7U, 8U};
const uint8_t USR_SECTOR_ID[MCU_NUM_SECTOR_USR] = {9U, 10U, 11U};

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
    // 内部 Flash 是存储器映射：直接 memcpy（按字拷贝，远快于逐字节 volatile 读）
    memcpy(data, (const void *)addr, len);
    return true;
}

// 以word写入 会自动补齐
static bool mf_write(FlashChipHandle h, uint32_t addr, const uint8_t *data, uint32_t len)
{
    (void)h;
    if (addr < FLASH_START_ADDR || (addr + len) > FLASH_END_ADDR)
        return false;
    HAL_FLASH_Unlock();
    uint32_t i = 0;

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
    if (sec_id >= MCU_NUM_SECTOR_USR)
        return false;

    if (HAL_FLASH_Unlock() != HAL_OK)
        return false;

    uint32_t s = USR_SECTOR_ID[sec_id];
    __disable_irq();
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sector_err = 0U;
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = (uint32_t)s;
    erase.NbSectors = 1U;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    if (HAL_FLASHEx_Erase(&erase, &sector_err) != HAL_OK)
    {
        __enable_irq(); // 失败也必须恢复中断，否则中断被永久关闭
        HAL_FLASH_Lock();
        return false;
    }
    __enable_irq();

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

    __disable_irq();

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
            __enable_irq();

            HAL_FLASH_Lock();
            return false;
        }
    }
    __enable_irq();

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

const tFlashDriverOps mcu_flash_driver_ops = {
    .init = NULL, // MCU Flash 无初始化
    .read = mf_read,
    .write = mf_write,
    .erase_addr = mf_erase_addr,
    .erase_sector = mf_erase_usr_sector,
    .get_sector_count = mf_get_usr_sector_count,
    .get_sector_addr = mf_get_usr_sec_addr,
    .get_sector_size = mf_get_usr_sec_size,
};

// ---------- 静态实例类型 ----------

// MCU Flash 实例：ops + 配置（起始地址 / 容量）
typedef struct
{
    uint32_t start_addr; // 配置：内部 Flash 起始地址
    uint32_t capacity;   // 配置：容量（字节）
} tMcuFlash;

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
// MCU App 初始化：复位向量表偏移至 BL 部分，中断必须恢复（重启后）
void mcu_app_init(void)
{
    SCB->VTOR = FLASH_START_ADDR | BL_SIZE;
    __enable_irq(); // 重启后中断必须恢复
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

// ---------- 静态实例 ----------

// MCU Flash 实例（内部介质无外设句柄，配置为起始地址与容量）
static tMcuFlash s_mcu = {
    .start_addr = FLASH_START_ADDR,
    .capacity = FLASH_CAPACITY,
};

FlashChipHandle mcu_flash_get_handle(void)
{
    return (FlashChipHandle)&s_mcu;
}

// 取 IAP 配置（组装层据此初始化 tIAP）：介质复用 mcu_flash_driver_ops，
// 分区表与平台跳转由本文件提供；整区校验由 abs 的 iap_verify_crc 完成。
tIAPConfig mcu_iap_config(eIAPtype type)
{
    iap_parts_init();

    tIAPConfig cfg = {
        .flash_ops = &mcu_flash_driver_ops,
        .flash_handle = mcu_flash_get_handle(),
        .parts = s_iap_parts,
        .part_count = IAP_PART_COUNT,
        .jump = mcu_iap_jump,
        .type = type,
    };
    return cfg;
}
