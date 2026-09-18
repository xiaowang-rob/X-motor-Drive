#ifndef __SMO_H
#define __SMO_H

#include "bsp_base.h"
#include "foc_core.h"

// 无感滑模观测器 (SMO) - 精简版
// 始终运行，外部直接读取结果，所有速度单位为电角速度 (rad/s)

// ========== 配置宏 ==========
#define SMO_USE_CURRENT_OBSERVER 1 // 0=直接用反馈电流测试，1=启用完整电流观测器

// 观测器配置参数
typedef struct
{
    float k_sl_base;         // 基础滑模增益 [10.0~30.0]
    float k_sl_min_ratio;    // 高速最小增益比例 [0.3~0.7]
    float vel_adapt_start; // 自适应起始电角速度 [rad/s]
    float vel_adapt_end;   // 自适应结束电角速度 [rad/s]
    float delta;             // 边界层厚度 [0.05~0.2]
    float min_vel_elec;    // 最低有效电角速度 [rad/s]
    float max_vel_elec;    // 最高有效电角速度 [rad/s]
    float emf_max;           // 反电动势限幅 [V]
} tSMO_Config;

// PLL 观测器参数
#define SMO_USE_PLL 1  // 1=PLL角度跟踪, 0=atan2+50%平滑
#define SMO_GAIN_BY_DUTY 1  // 1=基于电压, 0=基于速度

typedef struct {
    float theta_pll;  // PLL输出角度 [rad]
    float vel_pll;  // PLL输出电角速度 [rad/s]
    float kp;         // 比例增益
    float ki;         // 积分增益
    float dt;         // 时间步长 [s]
} tSmoPll;

// SMO 主结构体
typedef struct
{
    // === 电机参数 (识别后导入) ===
    float rs;
    float ld;
    float lq;
    float psi_f;
    float dt;
    float inv_l_eff; // 预计算：1/(L_avg + Rs*dt)

    // === 观测器配置 ===
    tSMO_Config cfg;

    u8 ts_tick;

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
    tSmoPll pll;

    // === 内部缓存 ===
    float k_sl_curr;
} tSMO;



// === 接口函数 ===
void smo_init(tMotor *motor);
void smo_reset(void);
void smo_set_config(tSMO_Config *cfg);
void smo_pll_init(tSmoPll *pll, float kp, float ki, float dt);
void smo_pll_update(tSmoPll *pll, float theta_obs_rad);
void smo_main_loop(float v_alpha, float v_beta, float i_alpha, float i_beta);

// === 数据获取 ===
float smo_get_theta(void);
float smo_get_vel(void);

#endif // __SMO_H