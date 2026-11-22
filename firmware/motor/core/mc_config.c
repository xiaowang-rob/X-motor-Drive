#include "mc_config.h"

#include <stddef.h>

tMotorTiming mc_timing_default(float dt) {
    return (tMotorTiming){dt, 10u, 10u, 50u, 100u};
}

tMcResult mc_config_validate(const tParameter *p, const tMotorTiming *t) {
    if (t->dt <= 0.0f || !t->cmd_div || !t->vel_div ||
        !t->pos_div || !t->slow_div)
        return MC_INVALID_CONFIG;
    const float nonnegative[] = {
        p->dclkp, p->dclki, p->qclkp, p->qclki, p->vlkp, p->vlki,
        p->plkp, p->plki, p->plkd, p->fwlkp, p->fwlki,
        p->motor_ke,
        p->mit_kp, p->mit_kd, p->mit_kpmax, p->mit_kdmax, p->mit_tmax,
        p->traj_tolerance
    };
    for (size_t i = 0; i < sizeof(nonnegative) / sizeof(nonnegative[0]); ++i)
        if (nonnegative[i] < 0.0f)
            return MC_INVALID_CONFIG;
    if (!p->motor_polepairs || p->motor_type > MOTOR_BLDC ||
        p->motor_rated_current <= 0.0f ||
        p->velocity_limit <= 0.0f ||
        p->cfalpha < 0.0f || p->cfalpha > 1.0f ||
        p->plalpha < 0.0f || p->plalpha > 1.0f ||
        p->pos_pidmode > POS_PID_PC || p->pos_limit_mode > POS_LIMIT_BOTH ||
        p->traj_mode > TRAJ_S_CURVE || p->rotor_enc > ENC_SINCOS ||
        p->sensorless > SENSORLESS_HFISMO ||
        p->mit_kp > p->mit_kpmax || p->mit_kd > p->mit_kdmax)
        return MC_INVALID_CONFIG;
    if (p->pos_limit_mode == POS_LIMIT_BOTH && p->pos_min_limit > p->pos_max_limit)
        return MC_INVALID_CONFIG;
    if (p->traj_mode != TRAJ_DISABLE &&
        (p->traj_limit_d1 <= 0.0f ||
         p->traj_limit_d2 <= 0.0f ||
         (p->traj_mode == TRAJ_S_CURVE &&
          (p->traj_limit_d3 <= 0.0f))))
        return MC_INVALID_CONFIG;
    return MC_OK;
}
