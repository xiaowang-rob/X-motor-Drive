#include "pid.h"

#include "math_fast.h"

// PI初始化
void pi_init(tPI *pi, float kp, float ki, float output_limit, float dt)
{
    memset(pi, 0, sizeof(tPI));
    pi->kp = kp;
    pi->dt = dt;
    pi->ki = ki * dt;
    if (pi->ki < 1e-6f)
        pi->integral_limit = output_limit;
    else
        pi->integral_limit = output_limit / pi->ki * 0.9f; // 留10%余量
    pi->output_limit = output_limit;
}

// PI更新（带抗积分饱和）
float pi_update(tPI *pi, float ref, float fb)
{
    float error = ref - fb;

    pi->integral += error;
    pi->integral = CLAMP(pi->integral, -pi->integral_limit, pi->integral_limit);

    // 3. 计算最终输出
    pi->output = pi->kp * error + pi->ki * pi->integral;

    pi->output = CLAMP(pi->output, -pi->output_limit, pi->output_limit);
    return pi->output;
}

// 重置PI状态
void pi_reset(tPI *pi)
{
    pi->integral = 0.0f;
    pi->output = 0.0f;
}

// PID初始化
void pid_init(tPID *pid, float kp, float ki_cont, float kd_cont,
              float output_limit, float alpha, float dt)
{
    memset(pid, 0, sizeof(tPID));

    pid->kp = kp;

    // 离散化转换
    pid->dt = dt;
    pid->ki = ki_cont * pid->dt;
    pid->kd = kd_cont / pid->dt;

    pid->output_limit = output_limit;
    // 积分限幅：与输出量纲对齐（output = ki * integral）
    pid->integral_limit = (pid->ki > 1e-6f) ? (output_limit / pid->ki) * 0.9f : output_limit;
    pid->derivative_limit = output_limit * 3.0f; // 微分限幅可略大

    pid->alpha = alpha;
}

// PID更新（含微分滤波）
float pid_update(tPID *pid, float ref, float fb)
{
    float error = ref - fb;

    //  2. 积分项（积分钳位抗饱和）
    pid->integral += error;
    pid->integral = CLAMP(pid->integral, -pid->integral_limit, pid->integral_limit);

    //  3. 微分项（对反馈微分 + 强滤波，避免设定值突变冲击）
    float derivative_raw = fb - pid->last_error;
    pid->derivative_filter = pid->alpha * derivative_raw + (1.0f - pid->alpha) * pid->derivative_filter;

    pid->derivative_filter = CLAMP(pid->derivative_filter, -pid->derivative_limit, pid->derivative_limit);

    // 4. 合成输出 + 硬限幅
    pid->output = pid->kp * error + pid->ki * pid->integral + pid->kd * pid->derivative_filter;

    pid->output = CLAMP(pid->output, -pid->output_limit, pid->output_limit);

    // 5. 更新状态（存反馈值，下次对 fb 微分）
    pid->last_error = fb;
    return pid->output;
}

// 重置PID状态
void pid_reset(tPID *pid)
{
    pid->integral = 0.0f;
    pid->derivative_filter = 0.0f;
    pid->last_error = 0.0f;
    pid->output = 0.0f;
}
