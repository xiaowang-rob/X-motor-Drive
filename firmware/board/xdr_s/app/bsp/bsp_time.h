#ifndef __BSP_TIME_H
#define __BSP_TIME_H

#include <stdint.h>

#define __DEBUG_TIME 1 // 是否启用调试

#if __DEBUG_TIME
#define TIME_INIT() bsp_time_init();
#define CREATE_TIME_SAMPLE(sample) tTimeSample sample;
#define TIME_SAMPLE_START(sample)       \
    {                                   \
        sample.us_last = bsp_time_us(); \
    }
#define TIME_SAMPLE_STOP(sample)                    \
    {                                               \
        sample.us = bsp_time_us() - sample.us_last; \
        sample.us_last = bsp_time_us();             \
    }

#else
#define TIME_INIT() NULL;
#define CREATE_TIME_SAMPLE(sample) NULL;
#define TIME_SAMPLE_START(sample) NULL;
#define TIME_SAMPLE_STOP(sample) NULL;
#endif
typedef struct
{
    uint32_t us;
    uint32_t us_last;
} tTimeSample; // us时间样本

void bsp_time_init(void);

uint32_t bsp_time_us(void);
uint32_t bsp_time_ms(void);

// 忙等（让出 CPU 给中断）；仅主循环上下文可用
void bsp_time_delay_ms(uint32_t ms);

// 关中断并返回进入前的 PRIMASK（供 bsp_critical_exit 精确恢复）
uint32_t bsp_critical_enter(void);
void bsp_critical_exit(uint32_t primask);

#endif // __BSP_TIME_H
