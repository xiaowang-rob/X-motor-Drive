#ifndef __MIT_H
#define __MIT_H

typedef struct
{
    // 可调参数
    float Kp;         // 刚度 (Nm/rad)
    float Kd;         // 阻尼 (Nm/(rad/s))
    float tau_ff_dyn; // 前馈动态扭矩 (Nm/rad)
    float tau_ff_sta; // 前馈静态扭矩 (Nm/rad)

    float J; // 转动惯量 (kg*m^2)
    float B; // 摩擦系数 (Nms/rad)

    // 限幅
    float tau_max; // 最大扭矩 (Nm)

} tMIT_HandleTypeDef;

void mit_init(float Kp, float Kd, float J, float B, float tau_max);
float mit_pos_update(float acc_ref, float pos_ref, float pos_fb, float vel_fb);
float mit_vel_update(float acc_ref, float vel_ref, float vel_fb);
float mit_track_update(float acc_ref, float tau_ff_sta, float pos_ref, float pos_fb, float vel_ref, float vel_fb);
void mit_config_static(float Kp, float Kd);

#endif
