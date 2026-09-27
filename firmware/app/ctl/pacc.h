#ifndef __PACC_H
#define __PACC_H

#include <stdint.h>
// 位置记录
typedef struct
{
    int32_t num_turns; // 圈数累积

    float last_angle; // 上一个机械角度
    float zero_angle; // 位置机械零点
    float max_pos;    // 位置机械最大值
    float min_pos;    // 位置机械最小值

} tPosAcc;

float pos_accumulate(tPosAcc *acc, float theta);
void pos_acc_set_zero(tPosAcc *acc, float zero_angle);
void pos_acc_set_max_min(tPosAcc *acc, float max_pos, float min_pos);

#endif