#ifndef __BSP_MCU_H
#define __BSP_MCU_H

// 对mcu的一些基本函数做bsp映射和拓展

#include <stdbool.h>
#include <stdint.h>

#include "main.h"

// 中断使能
#define irq_enable() __enable_irq()
// 中断失能
#define irq_disable() __disable_irq()
// 软件复位
#define system_reset() NVIC_SystemReset()

void bsp_time_init(void);

uint32_t bsp_time_ms(void);

uint32_t bsp_time_us(void);

// 忙等（让出 CPU 给中断）；仅主循环上下文可用
void bsp_time_delay_ms(uint32_t ms);

// 关中断并返回进入前的 PRIMASK（供 bsp_critical_exit 精确恢复）
uint32_t bsp_critical_enter(void);
void bsp_critical_exit(uint32_t primask);

#define __DEBUG_TIME 1 // 是否启用调试

#if __DEBUG_TIME
typedef struct {
    uint32_t us;
    uint32_t us_last;
} tTimeSample; // us时间样本

#define TIME_INIT() bsp_time_init();
#define CREATE_TIME_SAMPLE(sample) tTimeSample sample;
#define TIME_SAMPLE_START(sample)                                              \
    {                                                                          \
        sample.us_last = bsp_time_us();                                        \
    }
#define TIME_SAMPLE_STOP(sample)                                               \
    {                                                                          \
        sample.us = bsp_time_us() - sample.us_last;                            \
        sample.us_last = bsp_time_us();                                        \
    }

#else
#define TIME_INIT() NULL;
#define CREATE_TIME_SAMPLE(sample) NULL;
#define TIME_SAMPLE_START(sample) NULL;
#define TIME_SAMPLE_STOP(sample) NULL;
#endif

#endif // __BSP_MCU_H
