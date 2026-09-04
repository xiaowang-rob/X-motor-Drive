#include "timeIF.h"
#include "platform.h"

// 初始化DWT（Data Watchpoint and Trace）周期计数器的典型实现，主要用于性能测量（如统计代码执行时间）或微秒级精确延时 （只在调试阶段使用）。
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
    uint32_t cyccnt = DWT->CYCCNT; // 先快照再除，避免多读不一致
    return cyccnt / (SystemCoreClock / 1000000U);
}

void time_delay_ms(uint32_t ms) { HAL_Delay(ms); }
