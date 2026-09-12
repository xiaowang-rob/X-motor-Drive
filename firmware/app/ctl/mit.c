
#include "mit.h"
#include "math_fast.h"

tMIT_HandleTypeDef mit;

void mit_init(float Kp, float Kd, float J, float B, float tau_max)
{
    mit.Kp = Kp;
    mit.Kd = Kd;
    mit.tau_ff_dyn = 0.0f;
    mit.tau_ff_sta = 0.0f;
    mit.J = J;
    mit.B = B;
    mit.tau_max = tau_max;
}
// MIT 位置模式
float mit_pos_update(float acc_ref, float pos_ref, float pos_fb, float vel_fb)
{
    // 误差计算
    float pos_err = pos_ref - pos_fb;

    mit.tau_ff_dyn = mit.J * acc_ref;
    // 核心公式：τ = Kp·e_p + τ_ff
    float tau = mit.Kp * pos_err - mit.Kd * vel_fb + mit.tau_ff_dyn;

    //  输出限幅
    return CLAMP(tau, -mit.tau_max, mit.tau_max);
}

// MIT速度模式
float mit_vel_update(float acc_ref, float vel_ref, float vel_fb)
{
    // 误差计算
    float vel_err = vel_ref - vel_fb;
    // 计算前馈扭矩
    mit.tau_ff_dyn = mit.J * acc_ref + mit.B * vel_ref;
    // 核心公式：τ = Kd·e_v + τ_ff
    float tau = mit.Kd * vel_err + mit.tau_ff_dyn;
    //  输出限幅
    return CLAMP(tau, -mit.tau_max, mit.tau_max);
}

// MIT轨迹追踪模式 输入参数：acc_ref (加速度参考)，tau_ff_sta (静态前馈扭矩)，pos_ref (位置参考)，pos_fb (位置反馈)，vel_ref (速度参考)，vel_fb (速度反馈)。输出参数：tau (控制扭矩)
float mit_track_update(float acc_ref, float tau_ff_sta, float pos_ref, float pos_fb, float vel_ref, float vel_fb)
{
    // 误差计算
    float pos_err = pos_ref - pos_fb;
    float vel_err = vel_ref - vel_fb;

    mit.tau_ff_dyn = mit.J * acc_ref + mit.B * vel_ref;
     mit.tau_ff_sta = tau_ff_sta; // 静态前馈扭矩
    float tau_ff = mit.tau_ff_dyn;
    if (FABSF(tau_ff_sta) > 0.01f) // 静态前馈扭矩大于0.01时，默认认为使用静态前馈扭矩 禁用动态前馈扭矩
        tau_ff = mit.tau_ff_sta;

    // 核心公式：τ = Kp·e_p + Kd·e_v + τ_ff
    float tau = mit.Kp * pos_err + mit.Kd * vel_err + tau_ff;

    //  输出限幅
    return CLAMP(tau, -mit.tau_max, mit.tau_max);
}

// 配置静态参数函数，用于更新控制参数
void mit_config_static(float Kp, float Kd)
{
    mit.Kp = Kp;
    mit.Kd = Kd;
}
