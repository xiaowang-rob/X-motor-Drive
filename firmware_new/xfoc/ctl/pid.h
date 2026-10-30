#ifndef __PID_H
#define __PID_H

// PI控制器 （离散域）
typedef struct
{
    float dt;
    float kp, ki;
    float integral, integral_limit;
    float output_limit, output;
} tPI;

// PID控制器（含微分滤波）（离散域）
typedef struct
{
    float dt;
    float kp, ki, kd;
    float integral, integral_limit;
    float last_error;
    float derivative_filter, derivative_limit;
    float output, output_limit;
    float alpha;
} tPID;

void pi_init(tPI *pi, float kp, float ki, float output_limit, float dt);
float pi_update(tPI *pi, float ref, float fb);
void pi_reset(tPI *pi);
void pid_init(tPID *pid, float kp, float ki_cont, float kd_cont,
              float output_limit, float alpha, float dt);
float pid_update(tPID *pid, float ref, float fb);
void pid_reset(tPID *pid);

#endif