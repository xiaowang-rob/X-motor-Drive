// ============================================================
// IF_time.c — 时间基准服务（板级）
//
// DWT 周期计数器提供微秒级时间戳（便于性能测量）；
// 毫秒时间戳与延时走 HAL SysTick。
// ============================================================
#include "IF_time.h"

#include "main.h"

// 初始化 DWT（Data Watchpoint and Trace）周期计数器：用于微秒级时间戳
void time_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t time_get_ms(void)
{
    return HAL_GetTick();
}

uint32_t time_get_us(void)
{
    // 预计算每微秒周期数，避免热路径上每次做除法
    static uint32_t s_cycles_per_us = 0U;
    if (s_cycles_per_us == 0U)
        s_cycles_per_us = SystemCoreClock / 1000000U;

    uint32_t cyccnt = DWT->CYCCNT; // 先快照再除，避免多读不一致
    return cyccnt / s_cycles_per_us;
}

void time_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}
