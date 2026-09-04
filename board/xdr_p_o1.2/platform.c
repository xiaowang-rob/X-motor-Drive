// ============================================================
// plat.c — xdr_p_o1.2 板级服务实现
//
// 时间服务：ms = SysTick(HAL_GetTick)；us = DWT 周期计数/主频。
// 中断回调集中：随各驱动迁移到"直连"后，HAL_*Callback 将在此集中转发
// （当前过渡期仍由 hw/*.c 各自持有，待迁移后搬入）。
// ============================================================

#include "platform.h"

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

// ---- 板级固件服务 ----

void plat_enable_irq(void)
{
    __enable_irq();
}

void plat_disable_irq(void)
{
    __disable_irq();
}

void plat_set_vector_offset(void)
{
    SCB->VTOR = VECT_TABLE_OFFSET;
}

void plat_system_reset(void)
{
    NVIC_SystemReset();
}

void plat_jump_to_app(void)
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
}
