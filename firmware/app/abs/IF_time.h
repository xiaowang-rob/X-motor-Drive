#ifndef __IF_TIME_H
#define __IF_TIME_H

#include "device.h"

// ============================================================
// time.h — 时间操作
//
// ============================================================
#define __DEBUG_TIME 1 // 是否启用调试

#if __DEBUG_TIME
#define TIME_INIT() time_init();
#define CREATE_TIME_SAMPLE(sample) tTimeSample sample;
#define TIME_SAMPLE_START(sample)       \
    {                                   \
        sample.us_last = time_get_us(); \
    }
#define TIME_SAMPLE_STOP(sample)                    \
    {                                               \
        sample.us = time_get_us() - sample.us_last; \
        sample.us_last = time_get_us();             \
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

void time_init(void);
uint32_t time_get_ms(void);
uint32_t time_get_us(void);
void time_delay_ms(uint32_t ms);

#endif // __IF_TIME_H
