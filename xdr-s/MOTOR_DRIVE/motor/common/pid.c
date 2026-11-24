#include "pid.h"
#include "bsp_math.h"
#include <string.h>

void pi_init(tPI *p, float kp, float ki, float dt, float lo, float hi) {
    *p = (tPI){.kp = kp, .ki_dt = ki * dt, .minimum = lo, .maximum = hi};
}

void pi_reset(tPI *p) {
    p->integral = p->output = 0.0f;
}

float pi_update(tPI *p, float ref, float fb) {
    float error = ref - fb;
    float next = MATH_CLAMP(p->integral + p->ki_dt * error,
                            p->minimum, p->maximum);
    float proportional = p->kp * error;
    float unsaturated = proportional + next;
    if (!((unsaturated > p->maximum && error > 0.0f) ||
          (unsaturated < p->minimum && error < 0.0f)))
        p->integral = next;
    p->output = MATH_CLAMP(proportional + p->integral,
                           p->minimum, p->maximum);
    return p->output;
}


void pid_init(tPID *p, float kp, float ki, float kd, float alpha,
              float dt, float lo, float hi) {
    memset(p, 0, sizeof(*p));
    p->kp = kp;
    p->ki_dt = ki * dt;
    p->kd_over_dt = kd / dt;
    p->alpha = alpha;
    p->minimum = lo;
    p->maximum = hi;
}

void pid_reset(tPID *p) {
    p->integral = p->derivative = p->output = 0.0f;
    p->seeded = false;
}

float pid_update(tPID *p, float ref, float fb) {
    float error = ref - fb;
    float derivative = p->seeded ? p->last_fb - fb : 0.0f;
    p->last_fb = fb;
    p->seeded = true;
    p->derivative += p->alpha * (derivative - p->derivative);
    float next = MATH_CLAMP(p->integral + p->ki_dt * error,
                            p->minimum, p->maximum);
    float base = p->kp * error + p->kd_over_dt * p->derivative;
    /* 饱和方向的误差不继续积累，反向误差可立即解除饱和。 */
    float unsaturated = base + next;
    if (!((unsaturated > p->maximum && error > 0.0f) ||
          (unsaturated < p->minimum && error < 0.0f)))
        p->integral = next;
    unsaturated = base + p->integral;
    p->output = MATH_CLAMP(unsaturated, p->minimum, p->maximum);
    return p->output;
}
