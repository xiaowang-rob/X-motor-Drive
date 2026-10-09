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

/* 自适应滞环电压调节 */
typedef struct
{
    float i_hyst_lo; // 电流自适应下限 A
    float i_hyst_hi; // 电流自适应上限 A
    float v_hyst_lo; // 电压自适应下限 V
    float v_hyst_hi; // 电压自适应上限 V
    float v_step;    // 电压步长 V

} tVADA;
/* 最小二乘 */
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
    float sum_x1x1, sum_x1x2, sum_x1;
    float sum_x2x2, sum_x2;
    float sum_n;
    float sum_x1y, sum_x2y, sum_y;
    uint16_t n;
} tLS3D;

void ls3d_reset(tLS3D *a);
void ls3d_accum(tLS3D *a, float x1, float x2, float y);
bool ls3d_fit(const tLS3D *a, float *k1, float *k2, float *k3);

/* ============================================================
 * 开环电压三点差分测电阻
 * ============================================================ */

typedef struct
{
    tVADA vada[2];
    float cur_steady_err;         // 稳态电流可接受误差
    float steady_ticks;           // 稳态保持时间
    float rs_min;                 // 最小值
    float rs_max;                 // 最大值
    float rs_phase_diff_thr_coef; // 三相电阻最大相对偏差 例如 0.15f
} tTune_rs_ov_cfg;
// 开环电压三点差分测电阻上下文
typedef struct
{
    eTuneOneState state;

    tTune_rs_ov_cfg cfg;
    float ud_inj[2];  // 目标点电压值（V）
    float id_meas[2]; // 电流测量值（A）
    float rs_meas[3]; // 电阻测量值（R）
    float id_last;
    uint32_t steady_tick;
    uint8_t stage;

    struct
    {
        theta_e;
        ud;
    } cmd;

    float rs_out;

} tTune_rs_ov_ctx;

void tune_rs_ov_init(tTune_rs_ov_ctx *ctx, tTune_rs_ov_cfg cfg);
eTuneOneState tune_rs_ov_update(tTune_rs_ov_ctx *ctx,
                                float id, float ud);
/* ============================================================
 * 高频信号注入法 — dq轴电感在线辨识
 * 注入方式：d轴高频方波电压，同步解调提取电流幅值
 * ============================================================ */

typedef struct
{
    tVADA vada; // 电压自适应

    float omega_h;  // 高频信号注入角频率 ω_h (rad/s)
    float omega_dt; // 高频信号注入周期

    uint16_t n_per_cycle; // 每个注入周期计数
    uint32_t align_ticks; /* 对齐保持时长 tick */

    float ls_min;       // 电感最小值 H
    float ls_max;       // 电感最大值 H
    uint8_t avg_cycles; /* DFT 平均周期数，建议 4~8 */

} tTune_ls_hfi_cfg;

typedef struct
{
    eTuneOneState state;
    tTune_ls_hfi_cfg cfg;

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

} tTune_ls_hfi_ctx;

void tune_ls_hfi_init(tTune_ls_hfi_ctx *ctx, tTune_ls_hfi_cfg cfg);
eTuneOneState tune_ls_hfi_update(tTune_ls_hfi_ctx *ctx,
                                 float id, float iq, float ud, float uq);
/* ============================================================
 * sum线型拟合 — 编码器参数校准
 * ============================================================ */

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
    struct
    {
        uint8_t pole_pairs;
        bool direction;
        float theta_offset;
    } out;

} tEncCal_ctx;

void enc_cal_init(tEncCal_ctx *ctx, tEncCal_cfg cfg);
eTuneOneState enc_cal_update(tEncCal_ctx *ctx, float pos);

/* ============================================================
 * sum线型拟合 — 磁链参数辨识
 * ============================================================ */

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
} tTune_psif_cfg;

typedef struct
{
    eTuneOneState state;
    tTune_psif_cfg cfg;

    struct
    {
        float vel;
    } cmd;

    uint8_t step; /* 0=加速 1=采样 2=切点 3=拟合 4=完成 */
    uint8_t point_idx;
    float vel_target;

    uint32_t tick_cnt;
    tLS1D ls;
    struct
    {
        float psi_f;
        float ke;
    } out;
} tTune_psif_ctx;

void tune_psif_init(tTune_psif_ctx *ctx, tTune_psif_cfg cfg);
eTuneOneState tune_psif_update(tTune_psif_ctx *ctx,
                               float uq, float iq, float vel);

/* ============================================================
 * 转矩阶跃 + 3D 最小二乘 — J/B 辨识
 * J · dω/dt = Te − TL − B·ω
 * ============================================================ */

typedef struct
{
    float psi_f;          /* 已知永磁磁链 Wb */
    uint8_t pole_pairs;   // 已知极对数
    float iq_high;        /* 阶跃电流幅值 A */
    float vel_max;        /* 加速终止机械角速度 rad/s */
    float vel_settle;     /* 减速终止判定阈值 rad/s */
    float alpha_lpf;      /* 加速度低通滤波系数，建议 0.2~0.4 */
    uint32_t skip_ticks;  /* 阶跃后跳过周期（电流环上升） */
    uint32_t min_samples; /* 最少采样点数 */
} tTune_JB_cfg;

typedef struct
{
    eTuneOneState state;
    tTune_JB_cfg cfg;
    uint8_t step; /* 0=静止 1=加速 2=减速 3=拟合 4=完成 */

    struct
    {
        float iq;
    } cmd;

    uint32_t tick_cnt;
    float vel_prev;
    float alpha_filt;
    tLS3D ls;
    struct
    {
        float j, b, tl;
    } out;

} tTune_JB_ctx;

void tune_jb_init(tTune_JB_ctx *ctx, tTune_JB_cfg cfg);
eTuneOneState tune_jb_update(tTune_JB_ctx *ctx, float vel, float iq, float ts);

#endif