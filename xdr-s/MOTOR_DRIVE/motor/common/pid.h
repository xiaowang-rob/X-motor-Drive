#ifndef __PID_H
#define __PID_H

#include <stdbool.h>


typedef struct {
    float kp, ki_dt;
    float integral, output;
    float minimum, maximum;
} tPI;

void pi_init(tPI *pi, float kp, float ki, float dt,
             float minimum, float maximum);

void pi_reset(tPI *pi);

float pi_update(tPI *pi, float reference, float feedback);


typedef struct {
    float kp, ki_dt, kd_over_dt, alpha;
    float integral, derivative, last_fb, output;
    float minimum, maximum;
    bool seeded;
} tPID;


void pid_init(tPID *pid, float kp, float ki, float kd, float alpha,
              float dt, float minimum, float maximum);

void pid_reset(tPID *pid);

float pid_update(tPID *pid, float reference, float feedback);


#endif
