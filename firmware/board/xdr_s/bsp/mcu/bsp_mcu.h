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
    uint32_t task_us;
    uint32_t t_us;
    uint32_t start_us;
    uint32_t max_task_us;
    uint32_t max_t_us;
} tTimeSample; // us时间样本

#define TIME_INIT() bsp_time_init();
#define CREATE_TIME_SAMPLE(sample) tTimeSample sample;
#define TIME_SAMPLE_START(sample)                                              \
    {                                                                          \
        uint32_t new = bsp_time_us();                                          \
        if (new > sample.start_us) {                                           \
            sample.t_us = new - sample.start_us;                               \
            sample.start_us = new;                                             \
        } else {                                                               \
            sample.start_us = new;                                             \
        }                                                                      \
    }
#define TIME_SAMPLE_STOP(sample)                                               \
    {                                                                          \
        uint32_t new = bsp_time_us();                                          \
        if (new > sample.start_us) {                                           \
            sample.task_us = new - sample.start_us;                            \
            sample.max_task_us = sample.max_task_us > sample.task_us           \
                                     ? sample.max_task_us                      \
                                     : sample.task_us;                         \
        }                                                                      \
    }

#else
#define TIME_INIT() NULL;
#define CREATE_TIME_SAMPLE(sample) NULL;
#define TIME_SAMPLE_START(sample) NULL;
#define TIME_SAMPLE_STOP(sample) NULL;
#endif

#endif // __BSP_MCU_H
