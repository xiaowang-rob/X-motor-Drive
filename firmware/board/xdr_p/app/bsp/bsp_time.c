// ============================================================
// bsp_time.c — 时间基准与临界区（板级 bsp）
//
// 微秒时间戳走 DWT->CYCCNT（Cortex-M4 内核周期计数器），毫秒走 SysTick。
// 换算除数在 init 里一次算好，避免热路径上重复读 SystemCoreClock 做除法。
// ============================================================

#include "bsp_time.h"

#include "stm32f4xx_hal.h"

// 每微秒内核周期数（init 时算好；0 表示未初始化）
static uint32_t s_cycles_per_us = 0U;

void bsp_time_init(void)
{
    // 使能 DWT 周期计数器
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    uint32_t per_us = SystemCoreClock / 1000000U;
    s_cycles_per_us = (per_us == 0U) ? 1U : per_us;
}

uint32_t bsp_time_us(void)
{
    // 先快照再除，避免 CYCCNT 在两次读之间变化
    uint32_t cyccnt = DWT->CYCCNT;
    return cyccnt / s_cycles_per_us;
}

uint32_t bsp_time_ms(void)
{
    return HAL_GetTick();
}

void bsp_time_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

uint32_t bsp_critical_enter(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

void bsp_critical_exit(uint32_t primask)
{
    // 仅当进入前中断是开着的，才恢复为开
    if ((primask & 1U) == 0U)
        __enable_irq();
}
