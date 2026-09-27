#include "pacc.h"
#include "bsp_math.h"

// 位置累积
float pos_accumulate(tPosAcc *acc, float theta)
{
    float angle_delta = theta - acc->last_angle;
    if (angle_delta < -MATH_PI)
        acc->num_turns++;
    else if (angle_delta > MATH_PI)
        acc->num_turns--;
    acc->last_angle = theta;
    return (theta - acc->zero_angle) + acc->num_turns * MATH_2PI;
}

void pos_acc_set_zero(tPosAcc *acc, float zero_angle)
{
    acc->zero_angle = zero_angle;
    acc->num_turns = 0;
    acc->last_angle = zero_angle;
}

void pos_acc_set_max_min(tPosAcc *acc, float max_pos, float min_pos)
{
    acc->max_pos = max_pos;
    acc->min_pos = min_pos;
}
