#ifndef MC_MOTION_H
#define MC_MOTION_H

#include "command/mc_command.h"
#include "core/mc_config.h"

typedef struct {
    tCmdCfg cfg;
    tRef ref;
    float target_speed;
    float accel_step, decel_step;
    float phase;
} tMcMotion;

void motion_begin(tMcMotion *motion, const tCmdCfg *cfg, const tParameter *param,
                  const tRotor *rotor, const tMotorTiming *timing);
void motion_accept(tMcMotion *motion, const tCmdValue *cmd, const tParameter *param);
void motion_step(tMcMotion *motion);

static inline const tRef *motion_reference(const tMcMotion *motion) {
    return &motion->ref;
}

float motion_openloop_angle(tMcMotion *motion, float pwm_dt);

#endif
