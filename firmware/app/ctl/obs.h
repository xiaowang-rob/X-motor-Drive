#ifndef __OBS_H
#define __OBS_H

#include <stdint.h>
#include <stdbool.h>

#include "protocol.h"
#include "pll.h"

// 位置记录
typedef struct
{
    int32_t num_turns; // 圈数累积

    float last_angle; // 上一个机械角度
    float zero_angle; // 位置机械零点
    float max_pos;    // 位置机械最大值
    float min_pos;    // 位置机械最小值

} tPosAcc;

typedef struct
{

    eObsList type; // 观测器输入类型
    tPosAcc pacc;  // 位置记录
    tPLL pll;      // PLL

    float theta_e;
    float theta_m;
} tObs;

bool obs_init(tObs *obs);
bool obs_loop_task(tObs *obs);
void obs_tim_task(tObs *obs, float ts,
                  float *theta_e, float *vel, float *pos);

#endif