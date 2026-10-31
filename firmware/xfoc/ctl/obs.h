#ifndef __OBS_H
#define __OBS_H

#include <stdbool.h>
#include <stdint.h>

#include "parameter_list.h"
#include "pll.h"

// 位置记录
typedef struct {
    int32_t num_turns; // 圈数累积

    float last_angle; // 上一个机械角度
    float zero_angle; // 位置机械零点
    float max_pos;    // 位置机械最大值
    float min_pos;    // 位置机械最小值

} tPosAcc;

typedef struct {
    tParameter *param;        // 参数
    eObsList rotor_obs;       // 转子观测器
    eObsList abs_obs;         // 绝对位置观测器
    eSpiEncList spi_enc_chip; // spi编码器芯片
    tPosAcc pacc;             // 位置记录
    tPLL pll;                 // PLL

    float theta_e; // 电角度
    float theta_r; // 转子角度
    float vel;     // 转子速度
    float pos;     // 转子位置
} tObs;

bool obs_init(tObs *obs, tParameter *param);
bool obs_loop_task(tObs *obs);
void obs_tim_task(tObs *obs, float ts, float *theta_r, float *theta_e,
                  float *vel, float *pos);
void obs_set_zero(tObs *obs);
void obs_set_pos_limit(tObs *obs);

#endif