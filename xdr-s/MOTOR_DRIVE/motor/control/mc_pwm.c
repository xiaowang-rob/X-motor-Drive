#include "mc_pwm.h"
#include "bsp_math.h"

static inline tAb inv_park(tDq duty, float sin, float cos) {
    return (tAb){
        duty.d * cos - duty.q * sin,
        duty.d * sin + duty.q * cos
    };
}

static inline void inv_clarke(tAb modulation, float *u, float *v, float *w) {
    *u = modulation.alpha;
    float half_alpha = -0.5f * *u;
    float half_beta = MATH_SQRT3_2 * modulation.beta;
    *v = half_alpha + half_beta;
    *w = half_alpha - half_beta;
}

static inline void svpwm_update(tAb modulation, float phases[3]) {
    float u, v, w;
    inv_clarke(modulation, &u, &v, &w);
    float high = MATH_MAXF(u, MATH_MAXF(v, w));
    float low = MATH_MINF(u, MATH_MINF(v, w));
    float center = 0.5f - 0.5f * (high + low);

    phases[0] = MATH_CLAMP(u + center, 0.0f, 1.0f);
    phases[1] = MATH_CLAMP(v + center, 0.0f, 1.0f);
    phases[2] = MATH_CLAMP(w + center, 0.0f, 1.0f);
}

void mc_pwm_modulate(tDq duty, float sin, float cos, float phases[3]) {
    tAb duty_ab = inv_park(duty, sin, cos);
    duty_ab.alpha *= MATH_INSQRT3;
    duty_ab.beta *= MATH_INSQRT3;
    svpwm_update(duty_ab, phases);
}
