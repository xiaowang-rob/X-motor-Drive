#ifndef __TUNE_ALG_H
#define __TUNE_ALG_H

#include "xmath.h"
#include <stdbool.h>
#include <stdint.h>

// 所有 update: 先采集上一拍指令产生的反馈，再调用，最后施加新的 cmd。
// 终态保持到重新 init；终态指令清零。外部仍须独立执行过流/超速保护。
typedef enum {
    TO_RUNNING,
    TO_DONE,
    TO_DATA_NOISE,
    TO_DATA_INVALID,
    TO_DATA_IMBALANCE,
    TO_TIMEOUT,
} eTuneOneState;

/* duty 从 0.01 起，以 0.01 为固定步长；每次改变后等待 settle_ticks。
 * 首次超过 i_target 时锁定前一个未超过目标的占空比；1% 就超标则失败。
 * 达到 duty_max 仍未超过目标时由总超时退出，不伪造跨越点。
 */
typedef struct {
    float i_target_max;
    float duty_max;
    uint32_t settle_ticks;
} tDutyAdapt;

/* 中心化在线最小二乘；n 使用 uint32_t，避免高频长窗口溢出。 */
typedef struct {
    float mean_x, mean_y, sxx, sxy, syy;
    uint32_t n;
} tLS1D;
void ls1d_reset(tLS1D *a);
void ls1d_accum(tLS1D *a, float x, float y);
bool ls1d_fit(const tLS1D *a, float *k, float *b, float *mse);

typedef struct {
    float mean_x1, mean_x2, mean_y;
    float s11, s12, s22, s1y, s2y, syy;
    uint32_t n;
} tLS3D;
void ls3d_reset(tLS3D *a);
void ls3d_accum(tLS3D *a, float x1, float x2, float y);
bool ls3d_fit(const tLS3D *a, float *k1, float *k2, float *k3);

/* Rs/L 合并：仅自适应第二点 duty2，第一点 duty1=0.6*duty2。
 * 三方向差分后转到固定 α 方向保持对齐；Ld/Lq 注入幅值均为 duty2，
 * 不再单独自适应电感。保持偏置同为 duty2，合成峰值可能达到 2*duty2。
 * duty_max 因此不得超过 0.5。模型和局限见 .c。
 */
typedef struct {
    tDutyAdapt adapt;     // 自适应 占空比
    float cur_steady_err; // 电流稳态误差阈值
    uint32_t steady_ticks, align_ticks, timeout_ticks;
    float rs_min, rs_max, rs_phase_diff_thr_coef;
    float omega_h, omega_dt;
    uint16_t n_per_cycle;
    uint8_t avg_cycles;
    float ls_min, ls_max;
} tTune_rl_cfg;
typedef struct {
    eTuneOneState state;
    tTune_rl_cfg cfg;
    struct {
        float duty, theta_e;
    } cmd;
    uint8_t step, angle_idx, uidx;
    uint32_t tick_cnt, wait_tick, steady_tick;
    float duty2, duty_previous, id_last, sum_i, sum_u;
    bool duty_locked;
    float id_meas[2], ud_meas[2], rs_meas[3];
    float sum_re, sum_im, sum_ure, sum_uim, amp_sum, uamp_sum;
    uint16_t dft_cnt, cycle_cnt;
    struct {
        float rs, ldq[2];
    } out;
} tTune_rl_ctx;
void tune_rl_init(tTune_rl_ctx *ctx, tTune_rl_cfg cfg);
/* id/iq、ud/uq 均对应上一拍 cmd.theta_e；ud/uq 是实际限幅后电压 V。
 * 先采集再 update 再施加新 cmd；函数内部将两组 dq 反馈转回固定 αβ。
 * 外部执行 ud=cmd.duty*Vbus/√3, uq=0，不在算法内重构电压。
 */
eTuneOneState tune_rl_update(tTune_rl_ctx *ctx, float id, float iq, float ud,
                             float uq);

/* 编码器：θe=s*p*(pos-offset)，正反向线性拟合斜率和相位。
 * pos
 * 为同一零点下连续展开的机械弧度；对齐时寻找首次超标前的占空比，扫描保持该占空比。
 */
typedef struct {
    tDutyAdapt adapt;
    uint32_t align_ticks, timeout_ticks; // 每段超时
    float delta_e, sample_step_e, travel_pos;
    uint8_t pole_pairs_expected;
    float fit_max_mse;
} tEncCal_cfg;
typedef struct {
    eTuneOneState state;
    tEncCal_cfg cfg;
    struct {
        float theta_e, duty;
    } cmd;
    uint8_t step;
    uint32_t tick_cnt, elapsed_ticks, wait_tick;
    float duty_previous;
    bool duty_locked;
    float pos_start, pos_origin, theta_e_acc, theta_e_raw;
    tLS1D ls_fwd, ls_bwd;
    struct {
        uint8_t pole_pairs; // 电机极对数
        bool direction;     // true: 编码器正方向与电角正方向相同
        float theta_offset; // 机械零点偏移 rad，等价周期为 2π/p
    } out;
} tEncCal_ctx;
void enc_cal_init(tEncCal_ctx *ctx, tEncCal_cfg cfg);
eTuneOneState enc_cal_update(tEncCal_ctx *ctx, float pos, float id);

/* 磁链：稳态 id≈0 时 uq-Rs*iq=ωe*ψf；多速度带截距拟合斜率。
 * uq 是实际施加 q 轴电压反馈（V），不是开环控制指令。
 */
typedef struct {
    float rs_known, vel_low, vel_high;
    uint8_t num_points;
    uint32_t steady_ticks, sample_ticks, timeout_ticks; // 每点总超时
    float vel_band, fit_max_mse;                        // 拟合 MSE 单位 V²
    uint8_t pole_pairs;
} tTune_psif_cfg;
typedef struct {
    eTuneOneState state;
    tTune_psif_cfg cfg;
    struct {
        float vel;
    } cmd;
    uint8_t point_idx;
    uint32_t tick_cnt, elapsed_ticks, samples;
    tLS1D ls;
    struct {
        float psi_f, ke;
    } out;
} tTune_psif_ctx;
void tune_psif_init(tTune_psif_ctx *ctx, tTune_psif_cfg cfg);
// vel 已换到电角正方向的机械 rad/s；id≈0、无弱磁，由调用者保证。
eTuneOneState tune_psif_update(tTune_psif_ctx *ctx, float uq, float iq,
                               float vel);

/* J/B：Te=J*dω/dt+B*ω+TL，正负电流两段激励后带截距回归。
 * Te=1.5*p*ψf*iq；正向运动段内假设 TL 恒定、id≈0。
 */
typedef struct {
    float psi_f;
    uint8_t pole_pairs;
    float iq_high, vel_max, vel_settle, alpha_lpf;
    uint32_t skip_ticks, min_samples, timeout_ticks; // 每段总超时
} tTune_JB_cfg;
typedef struct {
    eTuneOneState state;
    tTune_JB_cfg cfg;
    uint8_t step;
    struct {
        float iq;
    } cmd;
    uint32_t tick_cnt, samples_accel, samples_decel;
    float vel_prev, alpha_filt, vel_filt, torque_filt;
    tLS3D ls;
    struct {
        float j, b, tl;
    } out;
} tTune_JB_ctx;
void tune_jb_init(tTune_JB_ctx *ctx, tTune_JB_cfg cfg);
// vel 与 iq 转矩正方向一致，ts>0，机械角速度单位 rad/s。
eTuneOneState tune_jb_update(tTune_JB_ctx *ctx, float vel, float iq, float ts);

#endif
