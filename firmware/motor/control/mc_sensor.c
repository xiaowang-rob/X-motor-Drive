#include "mc_sensor.h"
#include "xmath.h"
#include <string.h>

void sensor_init(tMcSensor *s, const tParameter *p, float dt) {
    memset(s, 0, sizeof(*s));
    /* TODO(parameter): PLL 带宽缺失；暂用 300 rad/s。 */
    float wn = XMINF(300.0f, 0.25f / dt);
    s->dt = dt;
    s->kp_dt = XMATH_SQRT2 * wn * dt;
    s->ki_dt = wn * wn * dt;
    s->dir = p->positive_dir ? 1.0f : -1.0f;
    s->polepairs = p->motor_polepairs;
    s->elec_scale = s->dir * s->polepairs;
    s->offset = XANGLE_2PI(p->theta_offset) * s->elec_scale;
}

bool sensor_update(tMcSensor *s, float raw, bool valid) {
    s->rotor.valid = valid;
    if (!valid)
        return false;
    /* 端口输入是单圈机械角 [0, 2pi)，不再重复归一化。 */
    if (!s->seeded) {
        s->angle = raw;
        s->seeded = true;
    } else {
        float delta = raw - s->raw;
        if (delta < -XMATH_PI)
            ++s->turns;
        else if (delta > XMATH_PI)
            --s->turns;
        float error = XANGLE_PI(raw - s->angle);
        s->vel += s->ki_dt * error;
        s->angle = XANGLE_2PI(s->angle + s->vel * s->dt + s->kp_dt * error);
    }
    s->raw = raw;
    s->rotor.theta = XANGLE_2PI(s->dir * s->angle);
    s->rotor.vel = s->dir * s->vel;
    s->rotor.pos = s->dir * ((raw - s->zero) + s->turns * XMATH_2PI);
    s->rotor.theta_e = XANGLE_2PI(s->angle * s->elec_scale - s->offset);
    s->rotor.vel_e = s->rotor.vel * s->polepairs;
    return true;
}

void sensor_zero(tMcSensor *s) {
    s->zero = s->raw;
    s->turns = 0;
    s->rotor.pos = 0.0f;
}
