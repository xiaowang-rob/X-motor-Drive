
#include "mit.h"
#include "math_fast.h"

// MIT模式初始化函数，输入参数：Kp (比例增益)，Kd (微分增益)，tau_ff_sta (静态补偿扭矩)， tau_max (最大控制扭矩)
void mit_init(tMIT *mit, float Kp, float Kd, float tau_ff_sta, float tau_max)
{
    mit->Kp = Kp;
    mit->Kd = Kd;
    mit->tau_ff_sta = 0.0f;
    mit->tau_max = tau_max;
}

// MIT模式 输入参数：tau_ff (前馈扭矩)，pos_ref (位置参考)，pos_fb (位置反馈)，vel_ref (速度参考)，vel_fb (速度反馈)。输出参数：tau (控制扭矩)
float mit_update(tMIT *mit, float tau_ff, float pos_ref, float pos_fb, float vel_ref, float vel_fb)
{
    // 误差计算
    float pos_err = pos_ref - pos_fb;
    float vel_err = vel_ref - vel_fb;
    // 核心公式：τ = Kp·e_p + Kd·e_v + τ_ff
    float tau = mit->Kp * pos_err + mit->Kd * vel_err + mit->tau_ff_sta + tau_ff;
    //  输出限幅
    return CLAMP(tau, -mit->tau_max, mit->tau_max);
}
