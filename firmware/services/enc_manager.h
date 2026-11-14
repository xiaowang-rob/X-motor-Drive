#ifndef __ENC_MANAGER_H
#define __ENC_MANAGER_H

#include "enc.h"
#include "parameters.h"
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
    bool enc_active; // 编码器激活

    tPLL pll;     // PLL
    tPosAcc pacc; // 位置累积

    float theta_e; // 电角度
    float theta_r; // 转子角度
    float vel;     // 转子速度
} tEncManager;

bool em_init(tParameter *param);
void em_loop_task(void);
void em_theta_process_task(float ts, float *th_e, float *th_r, float *vel);
#endif