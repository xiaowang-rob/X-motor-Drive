// ============================================================
// IF_time.c — 时间基准服务（板级，应用侧契约的板级实现）
//
// 应用侧只认 IF_time.h 的 time_* 接口；具体时基统一由 bsp_time
// 提供（DWT 微秒 / SysTick 毫秒），本文件只做转发，不再各自实现
// 一套 DWT 逻辑 —— 避免"时间从哪来"出现第二份定义。
// ============================================================
#include "IF_time.h"

#include "bsp_time.h"

void time_init(void)
{
    bsp_time_init();
}

uint32_t time_get_ms(void)
{
    return bsp_time_ms();
}

uint32_t time_get_us(void)
{
    return bsp_time_us();
}

void time_delay_ms(uint32_t ms)
{
    bsp_time_delay_ms(ms);
}
