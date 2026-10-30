#ifndef __TRAJECTORY_H
#define __TRAJECTORY_H

#include "parameter_list.h"
#include <stdbool.h>

//  配置参数
typedef struct {
    float limit_d1;  // 一阶限幅 [unit/s]
    float limit_d2;  // 二阶限幅 [unit/s²]
    float limit_d3;  // 三阶限幅 [unit/s³] (仅 S 型有效)
    float tolerance; // 到达容差 [unit]
    eTrajMode mode;

} tTraj_Config;

//  输出结果
typedef struct {
    float value;   // 核心输出：平滑后的值
    float rate_d1; // 一阶变化率
    float rate_d2; // 二阶变化率 (仅 S 型有输出) (用于前馈)
} tTraj_Out;

//  轨迹规划器
typedef struct {
    tTraj_Config cfg; // 配置参数
    tTraj_Out out;    // 输出结果
    float target;     // 目标值
    float current;    // 当前规划值
    float rate;       // 一阶变化率
    float rate_d2;    // 二阶变化率(仅 S 型需要)
    bool busy;

} tTraj;

//  核心 API
bool traj_init(tTraj *traj, tTraj_Config cfg);
void traj_reset(tTraj *traj, float current_value);
void traj_set_target(tTraj *traj, float target);
void traj_Update(tTraj *traj, float dt);

#endif //  __TRAJECTORY_H