#ifndef __HFI_H
#define __HFI_H

#include <stdbool.h>
#include <stdint.h>
#include "pll.h"

// ================= 数据结构 =================
typedef struct
{

    // 注入状态
    short inj_signal; // 当前注入极性 (+1/-1)
    uint8_t inj_count;
    uint8_t freq_ticks;

    bool init_flag; // 初始位置标志位
    // 信号分离
    float ialpha_z[2];
    float ibeta_z[2];
    float ialpha_h[2];
    float ibeta_h[2];
    float i_hf_alpha, i_hf_beta;

    // PLL
    float kp, ki;
    float theta_e; // 电角度 (rad)
    float vel_e;   //
    float pll_error;
    float pll_integrator;

    float vel_filtered; // 滤波后角速度

    // 初始位置
    float id_h;
    float id_z[2];
    float init_curr_pos; //  +Ud 脉冲响应电流幅值 [A]
    float init_curr_neg; //  -Ud 脉冲响应电流幅值 [A]

} tHFI_Handle;

extern tHFI_Handle g_hfi;

// ================= 函数声明 =================
void hfi_init(float pll_kp, float pll_ki);
void hfi_step(float ialpha, float ibeta, float *u_alpha_h, float *u_beta_h, float ts);
void hfi_detect_initial_position(float id, float *ualpha, float *ubeta);

void hfi_reset_initial_position(void);
float hfi_get_vel_elec(void);
float hfi_get_theta_elec(void);

#endif // __HFI_H