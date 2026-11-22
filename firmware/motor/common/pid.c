#include "pid.h"

#include <string.h>
#include "xmath.h"

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
    float next = XCLAMP(p->integral + p->ki_dt * error,
                                p->minimum, p->maximum);
    float base = p->kp * error + p->kd_over_dt * p->derivative;
    /* 饱和方向的误差不继续积累，反向误差可立即解除饱和。 */
    float unsaturated = base + next;
    if (!((unsaturated > p->maximum && error > 0.0f) ||
          (unsaturated < p->minimum && error < 0.0f)))
        p->integral = next;
    unsaturated = base + p->integral;
    p->output = XCLAMP(unsaturated, p->minimum, p->maximum);
    return p->output;
}

void pid_track(tPID *p, float applied) {
    if (p->ki_dt > 0.0f)
        p->integral = XCLAMP(p->integral + applied - p->output,
                              p->minimum, p->maximum);
    p->output = applied;
}
