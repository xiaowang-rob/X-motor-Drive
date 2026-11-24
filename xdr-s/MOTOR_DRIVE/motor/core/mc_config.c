#include "mc_config.h"

#include <stddef.h>

tMotorTiming mc_timing_default(float dt) {
    return (tMotorTiming){.dt = dt, .medium_div = 10u};
}

#ifndef NDEBUG
bool mc_config_valid(const tParameter *p, const tMotorTiming *t) {
    if (t->dt <= 0.0f || t->medium_div < MC_MEDIUM_MIN_DIV)
        return false;
    const float nonnegative[] = {
        p->dclkp, p->dclki, p->qclkp, p->qclki, p->vlkp, p->vlki,
        p->plkp, p->plki, p->plkd, p->fwlkp, p->fwlki,
        p->motor_ke,
        p->mit_kp, p->mit_kd, p->mit_kpmax, p->mit_kdmax, p->mit_tmax,
        p->traj_limit_d1, p->traj_limit_d2
    };
    for (size_t i = 0; i < sizeof(nonnegative) / sizeof(nonnegative[0]); ++i)
        if (nonnegative[i] < 0.0f)
            return false;
    if (!p->motor_polepairs || p->motor_type > MOTOR_BLDC ||
        p->motor_rated_current <= 0.0f ||
        p->velocity_limit <= 0.0f ||
        p->cfalpha < 0.0f || p->cfalpha > 1.0f ||
        p->plalpha < 0.0f || p->plalpha > 1.0f ||
        p->pos_pidmode > POS_PID_PC || p->pos_limit_mode > POS_LIMIT_BOTH ||
        p->rotor_enc > ENC_SINCOS ||
        p->sensorless > SENSORLESS_HFISMO ||
        p->mit_kp > p->mit_kpmax || p->mit_kd > p->mit_kdmax)
        return false;
    if (p->pos_limit_mode == POS_LIMIT_BOTH && p->pos_min_limit > p->pos_max_limit)
        return false;
    return true;
}
#endif
