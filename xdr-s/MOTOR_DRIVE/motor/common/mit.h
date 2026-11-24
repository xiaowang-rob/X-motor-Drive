#ifndef MC_MIT_H
#define MC_MIT_H

typedef struct {
    float kp, kd;
    float pos_err, vel_err, comp;
    float decay, tmax;
} tMIT;

/* kp/kd 与 tmax 已由指令/配置层限幅；周期和时间常数仅初始化时处理。 */
void mit_init(tMIT *mit, float kp, float kd, float tmax,
                 float dt, float tau);
void mit_reset(tMIT *mit);
void mit_config(tMIT *mit, float kp, float kd);
float mit_update(tMIT *mit, float pos_ref, float pos_fb,
                  float vref, float vel_fb, float tau_ff);

#endif
