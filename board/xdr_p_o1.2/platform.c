// ============================================================
// platform.c — xdr_p_o1.2 板级服务实现（v2 锚点）
//
// 时间服务：ms = SysTick(HAL_GetTick)；us = DWT 周期计数/主频。
// 中断回调集中：随各驱动迁移到"直连"后，HAL_*Callback 将在此集中转发
// （当前过渡期仍由 hw/*.c 各自持有，待迁移后搬入）。
// ============================================================

#include "platform.h"

// ---- 时间服务 ----

static uint32_t plat_get_ms(void)
{
    return HAL_GetTick();
}

static uint32_t plat_get_us(void)
{
    uint32_t cyccnt = DWT->CYCCNT; // 先快照再除，避免多读不一致
    return cyccnt / (SystemCoreClock / 1000000U);
}

void platform_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t platform_get_ms(void)
{
    return plat_get_ms();
}

uint32_t platform_get_us(void)
{
    return plat_get_us();
}

void platform_delay_ms(uint32_t ms)
{
    uint32_t t0 = plat_get_ms();
    while ((plat_get_ms() - t0) < ms)
    {
    }
}

// ---- 板级固件服务 ----

void platform_enable_irq(void)
{
    __enable_irq();
}

void platform_disable_irq(void)
{
    __disable_irq();
}

void platform_set_vector_offset(uint32_t offset)
{
    SCB->VTOR = offset;
}

void platform_system_reset(void)
{
    NVIC_SystemReset();
}

void platform_jump_to_addr(uint32_t addr)
{
    // 校验向量表位于内部 flash 区（粗略防护），随后关中断并跳转
    if ((addr & 0xFFF00000U) != 0x08000000U)
        return;

    platform_disable_irq();
    SCB->VTOR = addr;
    __set_MSP(*(volatile uint32_t *)addr); // 目标固件的栈顶
    typedef void (*p_fn)(void);
    p_fn jump = (p_fn)(*(volatile uint32_t *)(addr + 4U)); // 复位向量
    jump();
}
