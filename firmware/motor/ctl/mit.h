#ifndef __MIT_H
#define __MIT_H

typedef struct
{
    // 可调参数
    float Kp; // 刚度 (Nm/rad)
    float Kd; // 阻尼 (Nm/(rad/s))

    // 限幅
    float kp_max;
    float kd_max;
    float tau_max; // 最大扭矩 (Nm)

    // 变量
    float p_error, v_error;
    float tau_ff_bt; // 无扰切换补偿

} tMIT;

void mit_init(tMIT *mit, float Kp, float Kd, float Kpmax, float Kdmax, float tau_max);
void mit_config(tMIT *mit, float newKp, float newKd);
float mit_update(tMIT *mit, float tau_ff, float pos_ref, float pos_fb, float vel_ref, float vel_fb);

#endif
