#ifndef MC_TRAJECTORY_H
#define MC_TRAJECTORY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MC_TRAJ_DISABLED = 0,
    MC_TRAJ_TRAPEZOID,
    MC_TRAJ_S_CURVE
} tTrajMode;

typedef struct {
    tTrajMode mode;
    float vmax, amax, jmax, tol;
} tTrajCfg;

typedef struct {
    double value, vel, acceleration;
    double requested, endpoint, coeff[6], elapsed, duration, inv_time;
    float vmax, amax, jmax, tol;
    tTrajMode mode;
    bool busy, replan;
} tTraj;

void traj_init(tTraj *traj, const tTrajCfg *config,
                        double initial);
void traj_set_target(tTraj *traj, double ref);
/* 有界但较重的 S 型重规划，仅由前台 service 调用。 */
bool traj_plan(tTraj *traj, float dt);
/* 返回本次是否推进了轨迹。 */
bool traj_update(tTraj *traj, float dt);

#endif
