#include "mc_sensor.h"
#include "bsp_math.h"
#include <string.h>

static inline tAb clarke(float u, float v, float w) {
    return (tAb){u, (v - w) * MATH_INSQRT3};
}

static inline tDq park(tAb current, float sin, float cos) {
    return (tDq){
        current.alpha * cos + current.beta * sin,
        current.beta * cos - current.alpha * sin
    };
}

void sensor_init(tMcSensor *s, const tParameter *p, float dt) {
    memset(s, 0, sizeof(*s));
    /* TODO(parameter): PLL 带宽缺失；暂用 300 rad/s。 */
    float wn = MATH_MINF(300.0f, 0.25f / dt);
    s->dt = dt;
    s->current_alpha = p->cfalpha;
    s->cos = 1.0f;
    s->encoder_enabled = p->rotor_enc != ENC_NONE && p->sensorless == SENSORLESS_NONE;
    s->kp_dt = MATH_SQRT2 * wn * dt;
    s->ki_dt = wn * wn * dt;
    s->polepairs = p->motor_polepairs;
}

void sensor_angle(tMcSensor *s, float raw) {
    if (!s->encoder_enabled)
        return;
    s->angle_raw = raw;
    /* 端口输入是单圈机械角 [0, 2pi)，不再重复归一化。 */
    if (!s->seeded) {
        s->angle = s->angle_raw ;
        s->seeded = true;
    } else {
        // 位置累加
        float delta = raw - s->angle;
        if (delta < -MATH_PI)
            ++s->turns;
        else if (delta > MATH_PI)
            --s->turns;
        // pll
        float error = MATH_ANGLE_PI(delta);
        s->vel += s->ki_dt * error;
        s->angle = MATH_ANGLE_2PI(s->angle + s->vel * s->dt + s->kp_dt * error);
    }
    s->rotor.theta = s->angle;
    s->rotor.vel = s->vel;
    s->rotor.pos = ((raw - s->zero) + (float) s->turns * MATH_2PI);
    s->rotor.theta_e = MATH_ANGLE_2PI(s->angle * s->polepairs);
    s->rotor.vel_e = s->rotor.vel * s->polepairs;
}

void sensor_current(tMcSensor *s, const float current[3]) {
    float alpha = s->current_alpha;
    if (!s->current_seeded || alpha >= 1.0f) {
        for (int i = 0; i < 3; i++) {
            s->is[i] = current[i];
        }
        s->current_seeded = true;
    } else {
        for (int i = 0; i < 3; i++) {
            s->is[i] += alpha * (current[i] - s->is[i]);
        }
    }
    tAb current_ab = clarke(s->is[0], s->is[1], s->is[2]);
    s->i_dq = park(current_ab, s->sin, s->cos);
}

void sensor_set_electrical_angle(tMcSensor *s, float angle) {
    MATH_SIN_COS(angle, &s->sin, &s->cos);
}

void sensor_zero(tMcSensor *s) {
    s->zero = s->angle_raw;
    s->turns = 0;
    s->rotor.pos = 0.0f;
}
