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

} tMIT;

void mit_init(tMIT *mit, float Kp, float Kd, float tau_ff_sta, float tau_max);
float mit_update(tMIT *mit, float tau_ff, float pos_ref, float pos_fb, float vel_ref, float vel_fb);

#endif
