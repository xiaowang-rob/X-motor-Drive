#ifndef __MC_SENSOR_H
#define __MC_SENSOR_H

#include "pll.h"
#include <stdbool.h>
#include <stdint.h>

// 位置记录
typedef struct {
    int32_t num_turns; // 圈数累积

    float last_angle; // 上一个机械角度
    float zero_angle; // 位置机械零点
    float max_pos;    // 位置机械最大值
    float min_pos;    // 位置机械最小值

} tPosAcc;

typedef struct {
    tPLL pll;     // PLL
    tPosAcc pacc; // 位置累积

    float theta_r_raw; // 原始输入转子角度
    float theta_r;     // 转子角度
    float vel;         // 转子速度
    float pos;         // 转子位置
    float theta_e;     // 电角度

} tMcSensor;

void mc_sensor_init(tMcSensor *sen);
void mc_sensor_theta_process(tMcSensor *sen, float ts, uint8_t polepairs);
void mc_sensor_set_pos_zero(tMcSensor *sen);
void mc_sensor_set_pos_limit(tMcSensor *sen);

#endif