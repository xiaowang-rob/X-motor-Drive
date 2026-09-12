#ifndef __HFI_H
#define __HFI_H
#include "filter.h"
#include "bsp_base.h"

// ================= 配置参数 =================
// 具体数值依据见后文参数说明

// 直接以载波频率运行
#define HFI_VEL_E_BANDWIDTH 40.0f // 电转速带宽 (Hz) 大概电角速度 4.363 rad/s

#define HFI_INJ_VOLT_AMP 2.0f // 注入电压幅值 (V)
#define HFI_PLL_KP 50.0f      // PLL 比例增益
#define HFI_PLL_KI 1000.0f    // PLL 积分增益

#define HFI_INIT_VOLT 0.4f   // 初始辨识电压
#define HFI_MAX_VEL_E 2.618f // 最大电转速 (rad/s) 划分 HFI和SMO的界限, 原 150°/s

// ================= 数据结构 =================
typedef struct
{
    // 注入状态
    s8 inj_signal; // 当前注入极性 (+1/-1)
    u8 inj_count;
    u8 freq_ticks;

    bool init_flag; // 初始位置标志位
    // 信号分离
    float ialpha_z[2];
    float ibeta_z[2];
    float ialpha_h[2];
    float ibeta_h[2];
    float i_hf_alpha, i_hf_beta;

    // PLL
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
void hfi_init();
void hfi_step(float ialpha, float ibeta, float *u_alpha_h, float *u_beta_h);
void hfi_detect_initial_position(float id, float *ualpha, float *ubeta);

bool hfi_get_status(void);
void hfi_reset_initial_position(void);
float hfi_get_vel_elec(void);
float hfi_get_theta_elec(void);

#endif // __HFI_H