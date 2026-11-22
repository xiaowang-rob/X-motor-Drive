#include "mc_foc.h"
#include "xmath.h"
#include <string.h>

void foc_init(tMcFoc *f) {
    memset(f, 0, sizeof(*f));
}

bool foc_current(tMcFoc *f, const tSample *s, float alpha) {
    float phase[3];
    switch (s->phase_mask) {
    case MC_PHASE_U | MC_PHASE_V:
        phase[0] = s->current[0]; phase[1] = s->current[1];
        phase[2] = -phase[0] - phase[1];
        break;
    case MC_PHASE_U | MC_PHASE_W:
        phase[0] = s->current[0]; phase[2] = s->current[2];
        phase[1] = -phase[0] - phase[2];
        break;
    case MC_PHASE_V | MC_PHASE_W:
        phase[1] = s->current[1]; phase[2] = s->current[2];
        phase[0] = -phase[1] - phase[2];
        break;
    case MC_PHASE_ALL: {
        float mean = (s->current[0] + s->current[1] + s->current[2]) * (1.0f / 3.0f);
        for (unsigned i = 0; i < 3; ++i)
            phase[i] = s->current[i] - mean;
        break;
    }
    default:
        return false;
    }
    for (unsigned i = 0; i < 3; ++i)
        f->is[i] += alpha * (phase[i] - f->is[i]);
    f->i_ab.alpha = f->is[0];
    f->i_ab.beta = (f->is[1] - f->is[2]) * XMATH_INSQRT3;
    return true;
}

void foc_park(tMcFoc *f, float angle) {
    XSIN_COS(angle, &f->sin, &f->cos);
    f->i_dq.d = f->i_ab.alpha * f->cos + f->i_ab.beta * f->sin;
    f->i_dq.q = -f->i_ab.alpha * f->sin + f->i_ab.beta * f->cos;
}

/* 扇区由电压矢量确定；0/180 度分界归入下一扇区。 */
static uint8_t sector(float a, float b) {
    if (b == 0.0f)
        return a > 0.0f ? 1u : (a < 0.0f ? 4u : 0u);
    float line = XMATH_SQRT3 * a;
    if (b > 0.0f) {
        if (a >= 0.0f)
            return b < line ? 1u : 2u;
        return b > -line ? 2u : 3u;
    }
    if (a < 0.0f)
        return b > line ? 4u : 5u;
    return b < -line ? 5u : 6u;
}

void foc_pwm(tMcFoc *f, tDq u) {
    float a = (u.d * f->cos - u.q * f->sin) * XMATH_INSQRT3;
    float b = (u.d * f->sin + u.q * f->cos) * XMATH_INSQRT3;
    float phase[3] = {a, -0.5f * a + XMATH_SQRT3_2 * b,
                         -0.5f * a - XMATH_SQRT3_2 * b};
    /* 沿用原代码：既然已经得到扇区，就按扇区取最大/最小相。 */
    static const uint8_t max_phase[7] = {0, 0, 1, 1, 2, 2, 0};
    static const uint8_t min_phase[7] = {0, 2, 2, 0, 0, 1, 1};
    f->pwm.sector = sector(a, b);
    float offset = 0.5f - 0.5f * (phase[max_phase[f->pwm.sector]] +
                                 phase[min_phase[f->pwm.sector]]);
    for (unsigned i = 0; i < 3; ++i)
        f->pwm.duty[i] = XCLAMP(phase[i] + offset, 0.0f, 1.0f);
}
