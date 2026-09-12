#ifndef __SLOT_CON_H
#define __SLOT_CON_H

#include <stdint.h>
#include <stdbool.h>

// 多级时间槽控制服务

// ---------- 控制分频 ----------
#define FREQ_HIGH_LOOP 2    // 高环分频 (每个pwm周期执行两次 0 pwm下溢中断 1 pwm上溢中断)
#define FREQ_MEDIUM_LOOP 10 // 中环分频 (每FREQ_HIGH_LOOP*FREQ_MEDIUM_LOOP = 10个控制周期)
#define FREQ_LOW_LOOP 10    // 低环分频 (每FREQ_MEDIUM_LOOP*FREQ_LOW_LOOP = 100个控制周期)

// 频率分频控制（实现高中低三环分层分段时隙控制）
typedef struct
{
    uint8_t slot;
    void (*task)(void);
} tSlotTask;

// 每个分频的最后一个时间槽加计数并且处理低一级的任务 故最后一个时间槽一般不分配任务

const tSlotTask high_schedule[FREQ_HIGH_LOOP];
const tSlotTask medium_schedule[FREQ_MEDIUM_LOOP];
const tSlotTask low_schedule[FREQ_LOW_LOOP];
typedef struct
{
    uint8_t base_tic;
    uint8_t high_tic;
    uint8_t medium_tic;

    float t_high; // 高环周期
    float t_med;  // 中环周期
    float t_low;  // 低环周期
} tSlotCon;

void slot_con_init(tSlotCon *sc, uint32_t pwm_period);
void slot_con_update(tSlotCon *sc, uint8_t tic);

#endif