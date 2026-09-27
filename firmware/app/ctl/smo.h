#ifndef __SMO_H
#define __SMO_H

#include "pll.h"
#include "parameters.h"

// ========== 配置宏 ==========
#define SMO_USE_CURRENT_OBSERVER 1 // 0=直接用反馈电流测试，1=启用完整电流观测器

// 观测器配置参数
typedef struct
{
    float k_sl_base;       // 基础滑模增益 [10.0~30.0]
    float k_sl_min_ratio;  // 高速最小增益比例 [0.3~0.7]
    float vel_adapt_start; // 自适应起始电角速度 [rad/s]
    float vel_adapt_end;   // 自适应结束电角速度 [rad/s]
    float delta;           // 边界层厚度 [0.05~0.2]
    float min_vel_elec;    // 最低有效电角速度 [rad/s]
    float max_vel_elec;    // 最高有效电角速度 [rad/s]
    float emf_max;         // 反电动势限幅 [V]
} tSMO_Config;

// PLL 观测器参数
#define SMO_USE_PLL 1      // 1=PLL角度跟踪, 0=atan2+50%平滑
#define SMO_GAIN_BY_DUTY 1 // 1=基于电压, 0=基于速度

// SMO 主结构体
typedef struct
{
    // === 电机参数 (识别后导入) ===
    float rs;
    float ld;
    float lq;
    float psi_f;

    float inv_l_eff; // 预计算：1/(L_avg + Rs*dt)

    // === 观测器配置 ===
    tSMO_Config cfg;

    uint8_t ts_tick;

    // === 电流观测状态 ===
    float i_alpha_hat;
    float i_beta_hat;

    // === 反电动势状态 ===
    float e_alpha;
    float e_beta;
    float e_alpha_filt;
    float e_beta_filt;

    // === 角度速度输出 ===
    float theta_elec; // 电角度 [0~2π) rad
    float theta_prev;
    float vel_elec; // 电角速度 [rad/s]

    // === PLL 状态 ===
    tPLL pll;

    // === 内部缓存 ===
    float k_sl_curr;
} tSMO;

tSMO g_smo;
// === 接口函数 ===

void smo_init(tParameter *param, float ts);
void smo_reset(void);
void smo_set_config(tSMO_Config *cfg, float ts);
void smo_main_loop(float v_alpha, float v_beta,
                   float i_alpha, float i_beta, float ts);

// === 数据获取 ===
static inline float smo_get_theta(void) { return g_smo.theta_elec; }
static inline float smo_get_vel(void) { return g_smo.vel_elec; }
#endif // __SMO_H