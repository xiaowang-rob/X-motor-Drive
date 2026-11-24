#include "mit.h"

#include "bsp_math.h"

void mit_init(tMIT *m, float kp, float kd, float limit, float dt, float tau) {
    *m = (tMIT){.kp = kp, .kd = kd, .tmax = limit,
                 .decay = MATH_EXPF(-dt / tau)};
}

void mit_reset(tMIT *m) {
    m->pos_err = m->vel_err = m->comp = 0.0f;
}

void mit_config(tMIT *m, float kp, float kd) {
    m->comp += (m->kp - kp) * m->pos_err +
                       (m->kd - kd) * m->vel_err;
    m->kp = kp;
    m->kd = kd;
}

float mit_update(tMIT *m, float pos_ref, float pos_fb,
                  float vel_ref, float vel_fb, float ff) {
    m->pos_err = pos_ref - pos_fb;
    m->vel_err = vel_ref - vel_fb;
    float torque = m->kp * m->pos_err + m->kd * m->vel_err +
                   ff + m->comp;
    m->comp *= m->decay;
    if (MATH_FABSF(m->comp) < 1e-7f)
        m->comp = 0.0f;
    return MATH_CLAMP(torque, -m->tmax, m->tmax);
}
