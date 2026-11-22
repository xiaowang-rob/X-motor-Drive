#include "pi.h"

#include "xmath.h"

void pi_init(tPI *p, float kp, float ki, float dt, float lo, float hi) {
    *p = (tPI){.kp = kp, .ki_dt = ki * dt, .minimum = lo, .maximum = hi};
}

void pi_reset(tPI *p) {
    p->integral = p->output = 0.0f;
}

float pi_update(tPI *p, float ref, float fb) {
    float error = ref - fb;
    float next = XCLAMP(p->integral + p->ki_dt * error,
                                p->minimum, p->maximum);
    float proportional = p->kp * error;
    float unsaturated = proportional + next;
    if (!((unsaturated > p->maximum && error > 0.0f) ||
          (unsaturated < p->minimum && error < 0.0f)))
        p->integral = next;
    p->output = XCLAMP(proportional + p->integral,
                              p->minimum, p->maximum);
    return p->output;
}

void pi_track(tPI *p, float applied) {
    if (p->ki_dt > 0.0f)
        p->integral = XCLAMP(p->integral + applied - p->output,
                                    p->minimum, p->maximum);
    p->output = applied;
}
