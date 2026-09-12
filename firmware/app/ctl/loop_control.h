#ifndef __LOOP_CONTROL_H
#define __LOOP_CONTROL_H

#include "bsp_base.h"
#include "parameter_manager.h"
#include "usr_config.h"
// 频率分频控制（实现高中低三环分层分段更新）
typedef struct
{
    u8 base_tic;
    u8 high_tic;
    u8 medium_tic;
    u8 low_tic;
    bool high_update[FREQ_HIGH_LOOP];
    bool medium_update[FREQ_MEDIUM_LOOP];
    bool low_update[FREQ_LOW_LOOP];
    float t_high; // 高环周期
    float t_med;  // 中环周期
    float t_low;  // 低环周期
} tFrequencyDivision;

extern tFrequencyDivision g_freqD; // 分频器

void freq_div_init(void);
void freq_div_update(void);

#endif
