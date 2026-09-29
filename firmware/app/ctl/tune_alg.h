#ifndef __TUNE_ALG_H
#define __TUNE_ALG_H

#include "bsp_math.h"

#include <stdbool.h>
#include <stdint.h>

// 单项整定状态
typedef enum
{
    TO_RUNNING, // 运行中
    TO_DONE,
    TO_DATA_NOISE,     // 数据噪声大
    TO_DATA_INVALID,   // 数据不合理
    TO_DATA_IMBALANCE, // 数据不平衡
    TO_TIMEOUT,        // 整定超时
} eTuneOneState;

typedef struct
{
    float cur_1;                  // 差分电流 1 (A)
    float cur_2;                  // 差分电流 2 (A)
    uint32_t steady_ticks;        // 稳态保持时间
    float rs_min;                 // 最小值
    float rs_max;                 // 最大值
    float rs_phase_diff_thr_coef; // 三相电阻最大相对偏差 例如 0.15f
} tTune_rs_oc_cfg;
// 开环电流三点差分测电阻上下文
typedef struct
{
    eTuneOneState state;

    const tTune_rs_oc_cfg *cfg;

    float id;

    float rs_meas[3]; // 电阻测量值（R）
    float ud_meas[2]; // 目标点电压值（V）

    uint32_t steady_tick;
    uint8_t stage;

    struct
    {
        theta_e;
        id;
    } cmd;
    struct
    {
        float rs;
    } out;
} tTune_rs_oc_ctx;

/* ============================================================
 * 高频信号注入法 — dq轴电感在线辨识
 * 注入方式：d轴高频方波电压，同步解调提取电流幅值
 * ============================================================ */

/* ---- 参数配置 ---- */
#define HF_INJ_FREQ_HZ 500.0f /* 注入频率 (Hz) */
#define HF_INJ_AMP_V 15.0f    /* 注入电压幅值 (V) */
#define CTRL_FREQ_HZ 10000.0f /* 控制频率 (Hz) */
#define LPF_CUTOFF_HZ 100.0f  /* 低通滤波器截止频率 */

/* ---- 状态变量 ---- */
typedef struct
{
    /* 注入信号发生器 */
    float phase;     /* 注入相位累积 */
    float phase_inc; /* 每周期相位增量 */
    float inj_value; /* 当前注入值 */

    /* 低通滤波器 (一阶IIR) */
    float lpf_alpha;    /* 滤波系数 */
    float id_base_filt; /* d轴基波电流滤波值 */
    float iq_base_filt; /* q轴基波电流滤波值 */

    /* 同步解调 */
    float id_hf_sum;      /* d轴高频电流累加 */
    float iq_hf_sum;      /* q轴高频电流累加 */
    uint16_t demod_count; /* 解调累加计数 */

    /* 辨识结果 */
    float Ld_hat;  /* d轴电感估计值 (H) */
    float Lq_hat;  /* q轴电感估计值 (H) */
    float Ld_filt; /* 滤波后的Ld */
    float Lq_filt; /* 滤波后的Lq */

    /* 死区补偿 */
    float deadtime_comp; /* 死区补偿电压 */
} HFInjection_t;

/* ============================================================
 * 带遗忘因子的递推最小二乘法 (FF-RLS)
 * 辨识目标：定子电阻 Rs 和永磁体磁链 Psi_f
 * 辨识模型：基于q轴电压方程 y = phi^T * theta
 *   y     = uq - omega_e * Ld * id
 *   phi   = [-iq, -omega_e]^T
 *   theta = [Rs, Psi_f]^T
 * ============================================================ */

#define RLS_DIM 2        /* 待辨识参数个数 */
#define RLS_LAMBDA 0.98f /* 遗忘因子，典型范围0.95~0.99 */

typedef struct
{
    float theta[RLS_DIM];      /* 参数估计向量 [Rs, Psi_f] */
    float P[RLS_DIM][RLS_DIM]; /* 协方差矩阵 */
    float phi[RLS_DIM];        /* 回归向量 */
    float y;                   /* 当前观测值 */
    float K[RLS_DIM];          /* 增益向量 */
    float lambda;              /* 遗忘因子 */
    float lambda_min;          /* 遗忘因子下界 */
    uint32_t update_count;
} FFRLS_t;
/* ---- 获取辨识结果 ---- */
float FFRLS_GetRs(const FFRLS_t *rls) { return rls->theta[0]; }
float FFRLS_GetPsiF(const FFRLS_t *rls) { return rls->theta[1]; }

/* ============================================================
 * 扩展卡尔曼观测器 (EKO) — 负载转矩在线辨识
 * 状态向量: x = [omega_m, T_L]^T
 *   机械运动方程: J*d(omega_m)/dt = Te - T_L - B*omega_m
 *   负载转矩模型: d(T_L)/dt = 0 (缓变假设)
 * ============================================================ */

#include <math.h>

#define EKO_STATE_DIM 2 /* 状态维数: [omega_m, T_L] */

typedef struct
{
    float x[EKO_STATE_DIM];                /* 状态估计 [omega_hat, TL_hat] */
    float P[EKO_STATE_DIM][EKO_STATE_DIM]; /* 误差协方差矩阵 */
    float Q[EKO_STATE_DIM][EKO_STATE_DIM]; /* 过程噪声协方差 */
    float R;                               /* 测量噪声方差 (转速测量) */
    float J_hat;                           /* 转动惯量估计值 (由MRAS提供) */
    float B;                               /* 粘滞摩擦系数 */
    float Te;                              /* 电磁转矩 (由电流计算) */
    float omega_meas;                      /* 转速测量值 */
    float dt;                              /* 采样周期 */
    /* 自适应Q矩阵参数 */
    float q_adapt_gain;
    float innovation_prev;
} EKO_t;

/* ---- 获取负载转矩估计 ---- */
float EKO_GetLoadTorque(const EKO_t *eko) { return eko->x[1]; }
float EKO_GetSpeed(const EKO_t *eko) { return eko->x[0]; }

/* ============================================================
 * 模型参考自适应系统 (MRAS) — 转动惯量在线辨识
 * 参考模型: 基于机械运动方程
 *   d(omega_r)/dt = (Te - TL) / J
 * 可调模型: 用估计惯量 J_hat 重构转速
 * 自适应律: 基于Popov超稳定性理论或Lyapunov方法
 * ============================================================ */

#define MRAS_GAIN_MIN 0.1f
#define MRAS_GAIN_MAX 100.0f

typedef struct
{
    /* 可调模型状态 */
    float omega_adj; /* 可调模型转速估计 */
    float J_hat;     /* 转动惯量估计值 */

    /* 参考模型 */
    float omega_ref;  /* 参考模型转速 (来自实际测量或理想模型) */
    float omega_meas; /* 实际转速测量值 */

    /* 自适应律参数 */
    float gamma;         /* 自适应增益 */
    float gamma_min;     /* 增益下界 */
    float gamma_max;     /* 增益上界 */
    float integral_term; /* 积分项累积 */

    /* 输入 */
    float Te;     /* 电磁转矩 */
    float TL_hat; /* 负载转矩估计 (来自EKO) */
    float dt;     /* 采样周期 */

    /* 输出滤波 */
    float J_filt;     /* 滤波后的惯量估计 */
    float output_lpf; /* 输出低通滤波系数 */

    /* 收敛状态 */
    float error;
    float error_prev;
    uint32_t converge_count;
    uint8_t converged;
} MRAS_t;

/* ---- 获取辨识结果 ---- */
float MRAS_GetInertia(const MRAS_t *mras) { return mras->J_filt; }
float MRAS_GetInertiaRaw(const MRAS_t *mras) { return mras->J_hat; }
uint8_t MRAS_IsConverged(const MRAS_t *mras) { return mras->converged; }

#endif