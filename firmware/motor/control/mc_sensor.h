#ifndef MC_SENSOR_H
#define MC_SENSOR_H

#include "core/mc_types.h"
#include "parameter_list.h"

typedef struct {
    tRotor rotor;
    float raw, angle, vel, zero;
    float dt, kp_dt, ki_dt;
    float dir, polepairs, elec_scale, offset;
    int32_t turns;
    bool seeded;
} tMcSensor;

void sensor_init(tMcSensor *s, const tParameter *p, float dt);
bool sensor_update(tMcSensor *s, float raw, bool valid);
void sensor_zero(tMcSensor *s);

#endif
