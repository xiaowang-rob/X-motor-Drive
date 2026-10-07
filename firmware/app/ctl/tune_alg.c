#include "tune_alg.h"

/* ========== 最小二乘 ========== */

void ls1d_reset(tLS1D *a) { memset(a, 0, sizeof(*a)); }

void ls1d_accum(tLS1D *a, float x, float y)
{
    a->sum_x += x;
    a->sum_y += y;
    a->sum_xx += x * x;
    a->sum_xy += x * y;
    a->sum_yy += y * y;
    a->n++;
}

bool ls1d_fit(const tLS1D *a, float *k, float *b, float *mse)
{
    if (a->n < 10)
    {
        *k = 0;
        *b = 0;
        *mse = 1e9f;
        return false;
    }
    float den = a->n * a->sum_xx - a->sum_x * a->sum_x;
    if (fabsf(den) < 1e-9f)
    {
        *k = 0;
        *b = 0;
        *mse = 1e9f;
        return false;
    }
    *k = (a->n * a->sum_xy - a->sum_x * a->sum_y) / den;
    *b = (a->sum_y - (*k) * a->sum_x) / a->n;
    float n = (float)a->n;
    *mse = (a->sum_yy - 2 * (*k) * a->sum_xy - 2 * (*b) * a->sum_y + (*k) * (*k) * a->sum_xx + 2 * (*k) * (*b) * a->sum_x + (*b) * (*b) * n) / n;
    return true;
}

void ls3d_reset(tLS3D *a) { memset(a, 0, sizeof(*a)); }

void ls3d_accum(tLS3D *a, float x1, float x2, float y)
{
    a->sum_x1x1 += x1 * x1;
    a->sum_x1x2 += x1 * x2;
    a->sum_x1 += x1;
    a->sum_x2x2 += x2 * x2;
    a->sum_x2 += x2;
    a->sum_n += 1.0f;
    a->sum_x1y += x1 * y;
    a->sum_x2y += x2 * y;
    a->sum_y += y;
    a->n++;
}

/* 高斯消元解 3x3 线性方程组 */
static bool solve3x3(float A[3][3], float b[3], float x[3])
{
    float m[3][4];
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
            m[i][j] = A[i][j];
        m[i][3] = b[i];
    }
    for (int k = 0; k < 3; k++)
    {
        int piv = k;
        for (int i = k + 1; i < 3; i++)
            if (fabsf(m[i][k]) > fabsf(m[piv][k]))
                piv = i;
        if (fabsf(m[piv][k]) < 1e-9f)
            return false;
        if (piv != k)
        {
            for (int j = 0; j < 4; j++)
            {
                float tmp = m[k][j];
                m[k][j] = m[piv][j];
                m[piv][j] = tmp;
            }
        }
        for (int i = k + 1; i < 3; i++)
        {
            float f = m[i][k] / m[k][k];
            for (int j = k; j < 4; j++)
                m[i][j] -= f * m[k][j];
        }
    }
    for (int i = 2; i >= 0; i--)
    {
        float s = m[i][3];
        for (int j = i + 1; j < 3; j++)
            s -= m[i][j] * x[j];
        x[i] = s / m[i][i];
    }
    return true;
}

bool ls3d_fit(const tLS3D *a, float *k1, float *k2, float *k3)
{
    if (a->n < 10)
    {
        *k1 = *k2 = *k3 = 0;
        return false;
    }

    float A[3][3] = {
        {a->sum_x1x1, a->sum_x1x2, a->sum_x1},
        {a->sum_x1x2, a->sum_x2x2, a->sum_x2},
        {a->sum_x1, a->sum_x2, a->sum_n}};
    float b[3] = {a->sum_x1y, a->sum_x2y, a->sum_y};
    float x[3];

    if (!solve3x3(A, b, x))
    {
        *k1 = *k2 = *k3 = 0;
        return false;
    }
    *k1 = x[0];
    *k2 = x[1];
    *k3 = x[2];
    return true;
}

/* --- 开环电流三点差分测电阻--- */

// 开环角度闭环电流测三相电阻 (双点差分 × 3角度)
// 施加固定电角度 + d轴电流，用 d 电压幅值计算 Rs
// 三个电角度 (0°, 120°, 240°) 各 120° 间隔，分别以 U / V / W 相为主载流相
// 每个角度做两点差分
// ================== 电阻整定系数 ==================

void tune_rs_oc_init(tTune_rs_oc_ctx *ctx, tTune_rs_oc_cfg cfg)
{
    memset(ctx, 0, sizeof(tTune_rs_oc_ctx));
    ctx->cfg = cfg;
}

eTuneOneState tune_rs_oc_update(tTune_rs_oc_ctx *ctx,
                                float id, float ud)
{
    ctx->state = TO_RUNNING;

    if (ctx->stage >= 6)
    {
        float rs0 = ctx->rs_meas[0];
        float rs1 = ctx->rs_meas[1];
        float rs2 = ctx->rs_meas[2];

        float rs_max = (rs0 > rs1) ? ((rs0 > rs2) ? rs0 : rs2)
                                   : ((rs1 > rs2) ? rs1 : rs2);
        float rs_min = (rs0 < rs1) ? ((rs0 < rs2) ? rs0 : rs2)
                                   : ((rs1 < rs2) ? rs1 : rs2);
        float rs_avg = (rs0 + rs1 + rs2) / 3.0f;

        // 三相平均作为最终 Rs
        ctx->rs_out = rs_avg;
        ctx->state = TO_DONE;

        // 合理性检查
        if (rs_avg < ctx->cfg.rs_min || rs_avg > ctx->cfg.rs_max)
            ctx->state = TO_DATA_INVALID;

        // 三相电阻差异检查
        float diff_ratio = (rs_max - rs_min) / rs_avg;
        if (diff_ratio > ctx->cfg.rs_phase_diff_thr_coef)
            ctx->state = TO_DATA_IMBALANCE;

        // 复位
        ctx->stage = 0;
        ctx->cmd.id = 0.0f;
        ctx->cmd.theta_e = 0.0f;

        return ctx->state;
    }

    uint8_t angle_idx = ctx->stage / 2; // 0,1,2
    uint8_t cur_idx = ctx->stage % 2;   // 0=I1, 1=I2

    // 控制指令
    ctx->cmd.id = (cur_idx == 0) ? (ctx->cfg.cur_1)
                                 : (ctx->cfg.cur_2);
    ctx->cmd.theta_e = angle_idx * MATH_2PI / 3.0f;

    // 稳态判断：实际 id 反馈 vs id_ref
    float i_err = FABSF(id - ctx->cmd.id);

    if (i_err < ctx->cfg.cur_steady_err)
    {
        if (ctx->steady_tick >= ctx->cfg.steady_ticks)
        {
            if (cur_idx == 0)
            {
                // 记录第一点 ud
                ctx->ud_meas[0] = ud;
                ctx->stage++;
            }
            else
            {
                // 记录第二点 ud
                ctx->ud_meas[1] = ud;

                float delta_i = ctx->cfg.cur_2 - ctx->cfg.cur_1;
                float delta_u = ctx->ud_meas[1] - ctx->ud_meas[0];

                // 当前角度电阻：Δud / Δid
                ctx->rs_meas[angle_idx] = delta_u / (delta_i + 1e-6f);
                ctx->stage++;
            }

            ctx->steady_tick = 0;
        }
    }
    else
    {
        // 电流波动，重置稳态计数
        ctx->steady_tick = 0;
    }

    return ctx->state;
}
// 电感校准

void tune_ls_hfi_init(tTune_ls_hfi_ctx *ctx, tTune_ls_hfi_cfg cfg)
{
    memset(ctx, 0, sizeof(tTune_ls_hfi_ctx));
    ctx->cfg = cfg;
    if (ctx->cfg.n_per_cycle <= 0)
        ctx->cfg.n_per_cycle = 1;
}

/* DFT 单频点幅值: 2/N · sqrt(re² + im²) */
static inline float dft_mag(float sum_re, float sum_im, uint16_t n)
{
    if (n == 0)
        return 0.0f;
    float s = 2.0f / (float)n;
    float re = sum_re * s;
    float im = sum_im * s;
    return SQRTF(re * re + im * im);
}

/* 电压自适应: 电流小于下限 → 增大电压; 大于上限 → 减小电压
 * 返回 true=已进入目标区间(锁定), false=继续自适应 */
static inline bool v_adapt(tTune_ls_hfi_ctx *ctx, float i_avg)
{
    if (i_avg < ctx->cfg.i_hyst_lo &&
        ctx->v_inj < ctx->cfg.v_inj_max - ctx->cfg.v_inj_step)
    {
        ctx->v_inj += ctx->cfg.v_inj_step;
        return false;
    }
    if (i_avg > ctx->cfg.i_hyst_hi &&
        ctx->v_inj > ctx->cfg.v_inj_step)
    {
        ctx->v_inj -= ctx->cfg.v_inj_step;
        return false;
    }
    return true;
}
// 对齐d 等 注入d 计算 对齐q 等 注入q 计算 结束
eTuneOneState tune_ls_hfi_update(tTune_ls_hfi_ctx *ctx,
                                 float id, float iq, float ud, float uq)
{
    ctx->state = TO_RUNNING;

    // 角度 一直为0 注入ud或uq
    ctx->cmd.theta = 0.0f;

    switch (ctx->step)
    {
    /* ============ 0: 对齐+自适应电压============ */
    case 0:
    {
        ctx->cmd.udq[0] = 0.0f;
        ctx->cmd.udq[1] = 0.0f;
        if (v_adapt(ctx, id))
        { // 电压锁定
            if (++ctx->tick_cnt > ctx->cfg.align_ticks)
            {
                ctx->step = 1;
                ctx->tick_cnt = 0;
            }
        }
        else
        { // 自适应调节电压
            ctx->cmd.udq[ctx->uidx] = ctx->v_inj;
        }

        return ctx->state;
    }

    /* ============ 1: 断电等待，让电流衰减 ============ */
    case 1:
        ctx->cmd.udq[0] = 0.0f;
        ctx->cmd.udq[1] = 0.0f;

        if (++ctx->tick_cnt > ctx->cfg.align_ticks)
        {
            ctx->step = 2;
            ctx->tick_cnt = 0;
            ctx->inj_angle = 0.0f;
            ctx->sum_re = ctx->sum_im = 0.0f;
            ctx->dft_cnt = 0;
            ctx->amp_sum = 0.0f;
            ctx->cycle_cnt = 0;
            ctx->v_inj = ctx->cfg.v_inj_start;
        }
        return ctx->state;

    /* ============ 2:注入 ============ */
    case 2:
    {
        ctx->cmd.udq[0] = 0.0f;
        ctx->cmd.udq[1] = 0.0f;
        ctx->cmd.udq[ctx->uidx] = ctx->v_inj * FAST_SIN(ctx->inj_angle);

        /* DFT 累加 id */
        float ang = ctx->cfg.omega_h * (float)ctx->dft_cnt;
        float i_meas = ctx->uidx == 0 ? id : iq;
        ctx->sum_re += i_meas * FAST_COS(ang);
        ctx->sum_im += i_meas * FAST_SIN(ang);
        ctx->dft_cnt++;

        ctx->inj_angle += ctx->cfg.omega_h;
        if (ctx->inj_angle > MATH_2PI)
            ctx->inj_angle -= MATH_2PI;

        if (ctx->dft_cnt >= ctx->cfg.n_per_cycle)
        {
            float I_mag = dft_mag(ctx->sum_re, ctx->sum_im, ctx->cfg.n_per_cycle);
            ctx->amp_sum += I_mag;
            ctx->cycle_cnt++;
            ctx->sum_re = ctx->sum_im = 0.0f;
            ctx->dft_cnt = 0;

            if (ctx->cycle_cnt >= ctx->cfg.avg_cycles)
            {
                float I_avg = ctx->amp_sum / (float)ctx->cycle_cnt;

                if (v_adapt(ctx, I_avg))
                {
                    /* 电压锁定，计算 Ld = V / (ω·I) */
                    if (I_avg < 1e-6f)
                    {
                        ctx->state = TO_DATA_NOISE;
                        return ctx->state;
                    }
                    ctx->ldq_out[ctx->uidx] = ctx->v_inj / (ctx->cfg.omega_h * I_avg);

                    if (ctx->uidx == 0)
                    { // 重置测试lq
                        ctx->step = 0;
                        ctx->uidx = 1;
                    }
                    else
                    { // 结束
                        ctx->step = 3;
                        ctx->uidx = 0;
                    }
                    ctx->tick_cnt = 0;
                    ctx->amp_sum = 0.0f;
                    ctx->cycle_cnt = 0;
                    return ctx->state;
                }
                ctx->amp_sum = 0.0f;
                ctx->cycle_cnt = 0;
            }
        }
        return ctx->state;
    }
        // ============ 3: 结束 ============ */
    case 3:
        ctx->state = TO_DONE;
        /* 合理性检查 */
        if (ctx->ldq_out[0] < ctx->cfg.ls_min || ctx->ldq_out[0] > ctx->cfg.ls_max ||
            ctx->ldq_out[1] < ctx->cfg.ls_min || ctx->ldq_out[1] > ctx->cfg.ls_max)
        {
            ctx->state = TO_DATA_INVALID;
        }
        ctx->cmd.udq[0] = 0.0f;
        ctx->cmd.udq[1] = 0.0f;

        return ctx->state;
    }

    return ctx->state;
}

// 编码器校准

void enc_cal_init(tEncCal_ctx *ctx, tEncCal_cfg cfg)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->cfg = cfg;
    ls1d_reset(&ctx->ls_fwd);
    ls1d_reset(&ctx->ls_bwd);
}

eTuneOneState enc_cal_update(tEncCal_ctx *ctx, float pos)
{
    ctx->state = TO_RUNNING;
    switch (ctx->step)
    {

    /* ========== 状态 0: 对齐电角度0  ========== */
    case 0:
    {
        ctx->cmd.theta_e = 0.0f;
        ctx->cmd.id = ctx->cfg.i_tune;
        if (++ctx->tick_cnt >= ctx->cfg.align_ticks)
        {
            ctx->tick_cnt = 0;

            ctx->pos_start = pos;
            ctx->theta_e_acc = ctx->cmd.theta_e;
            ctx->theta_e_raw = ctx->theta_e_acc;
            if (!ctx->forward_done)
            {
                ctx->step = 1; // 正转
            }
            else if (!ctx->backward_done)
            {
                ctx->step = 2; // 反转
            }
            else
            {
                ctx->step = 3; // 拟合
            }
        }
        break;
    }
    /* ========== 状态 1: 正转扫描 ========== */
    case 1:
    {
        ctx->cmd.theta_e = ctx->theta_e_acc;
        ctx->cmd.id = ctx->cfg.i_tune;

        ctx->theta_e_acc += ctx->cfg.delta_e;
        if (fabFABSFsf(ctx->theta_e_acc - ctx->theta_e_raw) >= ctx->cfg.sample_step_e)
        {
            ctx->theta_e_raw = ctx->theta_e_acc;
            ls1d_accum(&ctx->ls_fwd, pos, ctx->theta_e_acc);
        }
        if (FABSF(pos - ctx->pos_start) > ctx->cfg.travel_pos)
        {
            ctx->forward_done = true;
            ctx->step = 0;
            ctx->tick_cnt = 0;
        }
        break;
    }
    /* ========== 状态 2: 反转扫描 ========== */
    case 2:
    {
        ctx->cmd.theta_e = ctx->theta_e_acc;
        ctx->cmd.id = ctx->cfg.i_tune;
        ctx->theta_e_acc -= ctx->cfg.delta_e;
        if (FABSF(ctx->theta_e_acc - ctx->theta_e_raw) >= ctx->cfg.sample_step_e)
        {
            ctx->theta_e_raw = ctx->theta_e_acc;
            ls1d_accum(&ctx->ls_bwd, pos, ctx->theta_e_acc);
        }
        if (FABSF(pos - ctx->pos_start) > ctx->cfg.travel_pos)
        {
            ctx->backward_done = true;
            ctx->step = 0;
            ctx->tick_cnt = 0;
        }
        break;
    }
    /* ========== 状态 3: 拟合 ========== */
    case 3:
    {
        float kf, bf, msef, kb, bb, mseb;
        ls1d_fit(&ctx->ls_fwd, &kf, &bf, &msef);
        ls1d_fit(&ctx->ls_bwd, &kb, &bb, &mseb);

        if (msef > ctx->cfg.fit_max_mse || mseb > ctx->cfg.fit_max_mse)
        {
            ctx->state = TO_DATA_INVALID; // 无效数据
            return ctx->state;
        }

        float p_est = 0.5f * (FABSF(kf) + FABSF(kb));
        ctx->pole_pairs = (uint8_t)(p_est + 0.5f);

        if (ctx->pole_pairs != ctx->cfg.pole_pairs_expected)
        {
            ctx->state = TO_DATA_IMBALANCE; // 极对数不匹配

            return ctx->state;
        }
        if ((kf > 0) != (kb > 0))
        {
            ctx->state = TO_DATA_INVALID;
            return ctx->state;
        }

        ctx->direction = (kf > 0);

        float o_est_f = -bf / kf;
        float o_est_b = -bb / kb;
        ctx->theta_offset = normalize_angle_2pi(0.5f * (o_est_f + o_est_b));

        ctx->step = 4;
        break;
    }
        /* ========== 状态 4: 对齐到 0 判定 180° ========== */
    case 4:
    {
        ctx->cmd.theta_e = MATH_PI;
        ctx->cmd.id = ctx->cfg.i_tune;
        if (++ctx->tick_cnt >= ctx->cfg.align_ticks)
        {
            ctx->tick_cnt = 0;
            bool need_180 = false;
            float diff = pos - ctx->pos_start;
            float dead_zone_m = MATH_PI / ctx->pole_pairs * 0.8f;
            if (FABSF(diff) > dead_zone_m)
            {
                need_180 = ((diff > 0) != ctx->direction);
            }
            else
            {
                ctx->state = TO_TIMEOUT;
                return ctx->state;
            }
            // 融合
            ctx->theta_offset -= need_180 ? MATH_PI / ctx->pole_pairs : 0.0f;
            ctx->theta_offset = normalize_angle_2pi(ctx->theta_offset);
            ctx->state = TO_DONE; // 完成
            ctx->step = 0;
        }
        break;
    }
    }

    return ctx->state;
}

// 磁链参数辨识
void tune_psif_init(tTune_psif_ctx *ctx, tTune_psif_cfg cfg)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->cfg = cfg;
    if (ctx->cfg.num_points < 2)
        ctx->cfg.num_points = 2;

    ls1d_reset(&ctx->ls);
}

eTuneOneState tune_psif_update(tTune_psif_ctx *ctx,
                               float uq, float iq, float vel)
{
    ctx->state = TO_RUNNING;

    uint8_t npts = ctx->cfg.num_points;
    float t = (float)ctx->point_idx / (float)(npts - 1);
    ctx->vel_target = ctx->cfg.vel_low + t * (ctx->cfg.vel_high - ctx->cfg.vel_low);
    ctx->cmd.vel = ctx->vel_target;

    switch (ctx->step)
    {

    /* ========== 状态 0: 加速到目标转速 ========== */
    case 0:
        if (fabsf(vel - ctx->vel_target) < ctx->cfg.vel_band)
        {
            ctx->step = 1;
            ctx->tick_cnt = 0;
        }
        break;

    /* ========== 状态 1: 稳态采样 ========== */
    case 1:
        if (++ctx->tick_cnt >= ctx->cfg.steady_ticks)
        {
            /* 稳态后开始采样 */
            float vel_e = vel * ctx->cfg.pole_pairs;
            if (fabsf(vel_e) > 1.0f)
            { /* 避免低速除零 */
                float y = uq - ctx->cfg.rs_known * iq;
                ls_slope_accum(&ctx->ls, vel_e, y);
            }
        }
        if (ctx->tick_cnt >= ctx->cfg.steady_ticks + ctx->cfg.sample_ticks)
        {
            ctx->point_idx++;
            ctx->tick_cnt = 0;
            if (ctx->point_idx >= npts)
            {
                ctx->step = 3;
            }
            else
            {
                ctx->step = 0;
            }
        }
        break;

    /* ========== 状态 3: 拟合 ========== */
    case 3:
    {
        float k;
        if (!ls_slope_fit(&ctx->ls, &k) || k < 1e-6f || k > 1.0f)
        {
            ctx->state = TO_DATA_INVALID;

            return ctx->state;
        }
        ctx->out.psi_f = k;
        /* Ke: 反电动势常数，单位 V/(rad/s mech) */
        ctx->out.ke = k * (float)ctx->cfg.pole_pairs;

        ctx->cmd.vel = 0;
        return ctx->state;
    }
    }

    return ctx->state;
}

/* ========== J/B 辨识主模块 ========== */

void tune_jb_init(tTune_JB_ctx *ctx, tTune_JB_cfg cfg)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->cfg = cfg;
    ls3d_reset(&ctx->ls);
}

eTuneOneState tune_jb_update(tTune_JB_ctx *ctx, float vel, float iq, float ts)
{
    ctx->state = TO_RUNNING;
    ctx->cmd.iq = 0.0f;

    /* 计算加速度（低通滤波） */
    float alpha_raw = (vel - ctx->vel_prev) / ts;
    ctx->vel_prev = vel;
    ctx->alpha_filt += ctx->cfg.alpha_lpf * (alpha_raw - ctx->alpha_filt);

    /* 电磁转矩 */
    float Te = 1.5f * (float)ctx->cfg.pole_pairs * ctx->cfg.psi_f * iq;

    switch (ctx->step)
    {

    /* ========== 状态 0: 静止等待 ========== */
    case 0:
        ctx->cmd.iq = 0.0f;
        if (fabsf(vel) < ctx->cfg.vel_settle)
        {
            if (++ctx->tick_cnt >= 500)
            { /* 约 50 ms 静止 */
                ctx->step = 1;
                ctx->tick_cnt = 0;
                ctx->vel_prev = vel;
                ctx->alpha_filt = 0.0f;
            }
        }
        else
        {
            ctx->tick_cnt = 0;
        }
        break;

    /* ========== 状态 1: 正向加速 ========== */
    case 1:
        ctx->cmd.iq = ctx->cfg.iq_high;
        ctx->tick_cnt++;

        if (ctx->tick_cnt > ctx->cfg.skip_ticks)
        {
            ls3d_accum(&ctx->ls, ctx->alpha_filt, vel, Te);
        }

        if (vel > ctx->cfg.vel_max)
        {
            ctx->step = 2;
            ctx->tick_cnt = 0;
        }

        /* 电机不动 → 异常 */
        if (ctx->tick_cnt > 20000 && fabsf(vel) < ctx->cfg.vel_settle)
        {
            ctx->state = TO_TIMEOUT;

            return ctx->state;
        }
        break;

    /* ========== 状态 2: 反向减速 ========== */
    case 2:
        ctx->cmd.iq = -ctx->cfg.iq_high;
        ctx->tick_cnt++;

        if (ctx->tick_cnt > ctx->cfg.skip_ticks)
        {
            ls3d_accum(&ctx->ls, ctx->alpha_filt, vel, Te);
        }

        if (fabsf(vel) < ctx->cfg.vel_settle &&
            ctx->tick_cnt > ctx->cfg.skip_ticks + 200)
        {
            ctx->step = 3;
        }
        break;

    /* ========== 状态 3: 拟合 ========== */
    case 3:
    {
        ctx->cmd.iq = 0.0f;
        ctx->state = TO_DONE;
        if (ctx->ls.n < ctx->cfg.min_samples)
        {
            ctx->state = TO_DATA_NOISE;

            return ctx->state;
        }

        float J, B, TL;
        if (!ls3d_fit(&ctx->ls, &J, &B, &TL))
        {
            ctx->state = TO_DATA_INVALID;

            return ctx->state;
        }

        /* 物理约束 */
        if (J < 1e-7f || J > 100.0f)
        {
            ctx->state = TO_DATA_INVALID;

            return ctx->state;
        }
        if (B < 0.0f)
            B = 0.0f; /* 摩擦系数非负 */

        ctx->out.j = J;
        ctx->out.b = B;
        ctx->out.tl = TL;

        ctx->step = 4;

        return ctx->state;
    }
    }

    return ctx->state;
}