#ifndef __TUNE_ALG_H
#define __TUNE_ALG_H

#include "bsp_math.h"

#include <stdbool.h>
#include <stdint.h>

// 单项整定状态
typedef enum
{
    TO_RUNNING,        // 运行中
    TO_DONE,           // 完成
    TO_DATA_NOISE,     // 数据噪声大
    TO_DATA_INVALID,   // 数据不合理
    TO_DATA_IMBALANCE, // 数据不平衡
    TO_TIMEOUT,        // 整定超时
} eTuneOneState;

/* ============================================================
 * 开环电流三点差分测电阻
 * ============================================================ */

typedef struct
{
    float cur_1;                  // 差分电流 1 系数
    float cur_2;                  // 差分电流 2 系数
    float cur_steady_err;         // 稳态电流 误差系数
    float steady_ticks;           // 稳态保持时间
    float rs_min;                 // 最小值
    float rs_max;                 // 最大值
    float rs_phase_diff_thr_coef; // 三相电阻最大相对偏差 例如 0.15f
} tTune_rs_oc_cfg;
// 开环电流三点差分测电阻上下文
typedef struct
{
    eTuneOneState state;

    tTune_rs_oc_cfg cfg;

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

    float rs_out;

} tTune_rs_oc_ctx;

void tune_rs_ol_cur_init(tTune_rs_oc_ctx *ctx, tTune_rs_oc_cfg cfg);
eTuneOneState tune_rs_ol_cur(tTune_rs_oc_ctx *ctx,
                             float id, float ud);
/* ============================================================
 * 高频信号注入法 — dq轴电感在线辨识
 * 注入方式：d轴高频方波电压，同步解调提取电流幅值
 * ============================================================ */

typedef struct
{
    float omega_h;  // 高频信号注入角频率 ω_h (rad/s)
    float omega_dt; // 高频信号注入周期

    uint16_t n_per_cycle; // 每个注入周期计数
    uint32_t align_ticks; /* 对齐保持时长 tick */

    float v_inj_start;  /* 注入电压起始幅值 V */
    float v_inj_max;    /* 注入电压上限 V */
    float v_inj_step;   /* 自适应步长 V */
    float i_hyst_lo;    /* 电流自适应下限 A */
    float i_hyst_hi;    /* 电流自适应上限 A */
    float ls_min;       // 电感最小值 H
    float ls_max;       // 电感最大值 H
    uint8_t avg_cycles; /* DFT 平均周期数，建议 4~8 */

} tTune_Ldq_hfi_cfg;

typedef struct
{
    eTuneOneState state;
    tTune_Ldq_hfi_cfg cfg;

    uint8_t step;
    uint8_t uidx; // 0-d 1-q
    struct
    {
        float theta; /* 当前给定电角度 rad */
        float udq[2];
    } cmd;

    uint32_t tick_cnt;

    float inj_angle; /* 注入相位 rad */
    float v_inj;     /* 当前注入电压幅值 V */

    float sum_re, sum_im; /* DFT 累加器 */
    uint16_t dft_cnt;

    float amp_sum;
    uint16_t cycle_cnt;
    float ldq_out[2];

} tune_Ldq_hfi_ctx;

/* ============================================================
 * sum线型拟合 — 编码器参数校准
 * ============================================================ */

/* 1D 最小二乘 */
typedef struct
{
    float sum_x, sum_y, sum_xx, sum_xy, sum_yy;
    uint16_t n;
} tLS1D;

void ls1d_reset(tLS1D *a);
void ls1d_accum(tLS1D *a, float x, float y);
bool ls1d_fit(const tLS1D *a, float *k, float *b, float *mse);

typedef struct
{
    float i_tune; /* 校准电流 A */

    uint32_t align_ticks;        /* 对齐保持 tick */
    float delta_e;               /* 每次调用电角度增量 rad */
    float sample_step_e;         /* 采样步长 rad 电角度，建议 0.1745 */
    float travel_pos;            /* 拖动行程 rad 机械角度，建议 6.3 */
    uint8_t pole_pairs_expected; /* 期望极对数，用于校验 */
    float fit_max_mse;           /* 拟合质量阈值 */

} tEncCal_cfg;

typedef struct
{
    eTuneOneState state;

    tEncCal_cfg cfg;

    struct
    {
        float theta_e;
        float id;
    } cmd;

    uint8_t step;

    uint32_t tick_cnt;

    float pos_start;
    float theta_e_acc;
    float theta_e_raw;

    bool forward_done;
    bool backward_done;

    tLS1D ls_fwd;
    tLS1D ls_bwd;

    uint8_t pole_pairs;
    bool direction;
    float theta_offset;

} tEncCal_ctx;

void enc_cal_init(tEncCal_ctx *ctx, tEncCal_cfg cfg);

typedef struct
{
    float rs_known;        /* 已知相电阻 Ω */
    float vel_low;         /* 最低机械转速 rad/s */
    float vel_high;        /* 最高机械转速 rad/s */
    uint8_t num_points;    /* 转速点数量，建议 4~6 */
    uint32_t steady_ticks; /* 每点稳态等待 tick */
    uint32_t sample_ticks; /* 每点采样 tick */
    float vel_band;        /* 转速到位判定带宽 rad/s */
    uint8_t pole_pairs;    /* 极对数（编码器校准输出） */
} tPsif_cfg;

typedef struct
{
    eTuneOneState state;
    tPsif_cfg cfg;

    struct
    {
        float vel;
    } cmd;

    uint8_t step; /* 0=加速 1=采样 2=切点 3=拟合 4=完成 */
    uint8_t point_idx;
    float vel_target;

    uint32_t tick_cnt;
    tLS1D ls;

    float psi_f;
    float ke;
} tPsif_ctx;

#endif