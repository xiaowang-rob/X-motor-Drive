
#include "mit.h"
#include "xmath.h"

// MIT模式初始化函数，输入参数：Kp (比例增益)，Kd (微分增益)，tau_ff_sta
// (静态补偿扭矩)， tau_max (最大控制扭矩)
void mit_init(tMIT *mit, float Kp, float Kd, float Kpmax, float Kdmax,
              float tau_max) {
    mit->Kp = Kp;
    mit->Kd = Kd;
    mit->kp_max = Kpmax;
    mit->kd_max = Kdmax;
    mit->tau_max = tau_max;
}
void mit_config(tMIT *mit, float newKp, float newKd) {
    // 无扰切换 Bumpless Transfer
    // 用前馈力矩补偿掉增益跳变引起的力矩差，保证总力矩连续，然后让的补偿量慢慢衰减回零
    mit->tau_ff_bt =
        (mit->Kp - newKp) * mit->p_error + (mit->Kd - newKd) * mit->v_error;
}
// MIT模式 输入参数：tau_ff (前馈扭矩)，pos_ref (位置参考)，pos_fb
// (位置反馈)，vel_ref (速度参考)，vel_fb (速度反馈)。输出参数：tau (控制扭矩)
float mit_update(tMIT *mit, float tau_ff, float pos_ref, float pos_fb,
                 float vel_ref, float vel_fb) {
    // 误差计算
    mit->p_error = pos_ref - pos_fb;
    mit->v_error = vel_ref - vel_fb;
    // 核心公式：τ = Kp·e_p + Kd·e_v + τ_ff
    float tau = mit->Kp * mit->p_error + mit->Kd * mit->v_error + tau_ff +
                mit->tau_ff_bt;
    // 无扰切换 补偿衰减
    if (mit->tau_ff_bt > 0.01f)
        mit->tau_ff_bt -= 0.01f;
    else if (mit->tau_ff_bt < 0.01f)
        mit->tau_ff_bt += 0.01f;
    else
        mit->tau_ff_bt = 0;
    //  输出限幅
    return XCLAMP(tau, -mit->tau_max, mit->tau_max);
}
