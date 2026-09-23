#ifndef __BSP_TIME_H
#define __BSP_TIME_H

#include <stdint.h>

// ============================================================
// bsp_time.h — 时间基准与临界区（板级 bsp）
//
// 全工程唯一的时间来源：驱动与应用都经此取时，不得直接调 HAL_GetTick /
// HAL_Delay —— 否则"时间从哪来"这件事会散落到每个文件里。
//
//   bsp_time_us()   DWT 周期计数器换算，微秒级，用于性能测量与时间戳
//   bsp_time_ms()   SysTick 毫秒
//   bsp_time_init() 使能 DWT，上电调用一次（其他接口依赖它）
//
// 临界区：进出配对，传入进入时的 PRIMASK，支持嵌套调用后的正确恢复。
//
// 上下文约束：
//   bsp_time_us/ms、bsp_critical_*  可在中断上下文使用
//   bsp_time_delay_ms               仅主循环上下文（依赖 SysTick 中断喂数）
// ============================================================

void bsp_time_init(void);

uint32_t bsp_time_us(void);
uint32_t bsp_time_ms(void);

// 忙等（让出 CPU 给中断）；仅主循环上下文可用
void bsp_time_delay_ms(uint32_t ms);

// 关中断并返回进入前的 PRIMASK（供 bsp_critical_exit 精确恢复）
uint32_t bsp_critical_enter(void);
void bsp_critical_exit(uint32_t primask);

#endif // __BSP_TIME_H
