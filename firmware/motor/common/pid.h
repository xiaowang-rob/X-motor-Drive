#ifndef MC_PID_H
#define MC_PID_H

#include <stdbool.h>

typedef struct {
    float kp, ki_dt, kd_over_dt, alpha;
    float integral, derivative, last_fb, output;
    float minimum, maximum;
    bool seeded;
} tPID;

/* 同 PI 的调用前提；dt/输出范围在初始化时确定，微分对反馈作负反馈。 */
void pid_init(tPID *pid, float kp, float ki, float kd, float alpha,
                 float dt, float minimum, float maximum);
void pid_reset(tPID *pid);
float pid_update(tPID *pid, float reference, float feedback);
void pid_track(tPID *pid, float applied);

#endif
