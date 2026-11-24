#ifndef MC_SENSOR_H
#define MC_SENSOR_H

#include "core/mc_types.h"
#include "parameter_list.h"

typedef struct {
    tRotor rotor;
    float is[3];
    tDq i_dq;
    float sin, cos;
    float current_alpha;
    bool current_seeded;
    float angle_raw, angle, vel, zero;
    float dt, kp_dt, ki_dt;
    float  polepairs, offset;
    int32_t turns;
    bool seeded, encoder_enabled;
} tMcSensor;

void sensor_init(tMcSensor *s, const tParameter *p, float dt);
void sensor_angle(tMcSensor *s, float raw);
/* Park 使用缓存的正余弦。更新电角度后，调制使用新的正余弦。 */
void sensor_current(tMcSensor *s, const float current[3]);
void sensor_set_electrical_angle(tMcSensor *s, float angle);
void sensor_zero(tMcSensor *s);

#endif
