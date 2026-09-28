#include "tune_alg.h"

/* --- 开环电流三点差分测电阻--- */

// 开环电流测三相电阻 (双点差分 × 3角度)
// 在 OPEN_CUR 模式下，施加固定电角度 + q 轴电流，用 αβ 电压幅值计算 Rs
// 三个电角度 (270°, 30°, 150°) 各 120° 间隔，分别以 U / V / W 相为主载流相
// 每个角度做两点差分：I₁ = tune_cur × 0.2, I₂ = tune_cur × 0.6
eTuneOneState tune_rs_ol_cur(tTune_rs_oc_ctx *ctx,
                             float id, float ud)
{
    // stage: 0=Id1@U, 1=Id2@U; 2=Id1@V, 3=Id2@V; 4=Id1@W, 5=Id2@W; 6=done
    uint8_t stage = ctx->ol_stage;

    if (stage >= 6)
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
        ctx->out.rs = rs_avg;
        ctx->state = TO_DONE;

        // 合理性检查
        if (rs_avg < ctx->cfg.rs_min || rs_avg > ctx->cfg.rs_max)
            ctx->state = TO_DATA_INVALID;

        // 三相电阻差异检查
        float diff_ratio = (rs_max - rs_min) / rs_avg;
        if (diff_ratio > ctx->cfg.rs_phase_diff_thr_coef)
            ctx->state = TO_DATA_IMBALANCE;

        // 复位
        ctx->ol_stage = 0;
        ctx->cmd.id = 0.0f;
        ctx->cmd.theta_e = 0.0f;

        return ctx->state;
    }

    uint8_t angle_idx = stage / 2; // 0,1,2
    uint8_t cur_idx = stage % 2;   // 0=I1, 1=I2

    float id_ref = (cur_idx == 0) ? (ctx->cfg.cur_1)
                                  : (ctx->cfg.cur_2);

    // 注意：这里必须是 d 轴电流给定，iq_ref = 0
    ctx->cmd.id = id_ref;
    ctx->cmd.theta_e = angle_idx * MATH_2PI / 3.0f;

    // 稳态判断：实际 id 反馈 vs id_ref
    float i_err = FABSF(id - id_ref);
    float steady_thr = ctx->cfg.cur_2 * 0.02f;

    if (i_err < 0.2f)
    {
        if (ctx->steady_tick >= RS_STEADY_TICKS)
        {
            if (cur_idx == 0)
            {
                // 记录第一点 ud
                ctx->rs_ctx.ol_u[0] = ud;
                ctx->rs_ctx.ol_stage++;
            }
            else
            {
                // 记录第二点 ud
                ctx->rs_ctx.ol_u[1] = ud;

                float I1 = cur_lim * RS_I_TARGET_1_COEF;
                float I2 = cur_lim * RS_I_TARGET_2_COEF;
                float delta_i = I2 - I1;
                float delta_u = ctx->rs_ctx.ol_u[1] - ctx->rs_ctx.ol_u[0];

                // 信噪比检查
                if (FABSF(delta_i) < cur_lim * RS_MIN_DELTA_I_COEF)
                {
                    ctx->fault = FAULT_TUNE_CURRENT_VIBRATION;
                    return true;
                }

                // 当前角度电阻：Δud / Δid
                ctx->rs_ctx.ol_rs[angle_idx] = delta_u / (delta_i + 1e-6f);
                ctx->rs_ctx.ol_stage++;
            }

            ctx->steady_tick = 0;
        }
    }
    else
    {
        // 电流波动，重置稳态计数
        ctx->steady_tick = 0;
    }

    return false;
}
/* ---- 初始化 ---- */
void HFInjection_Init(HFInjection_t *hf)
{
    hf->phase = 0.0f;
    hf->phase_inc = MATH_2PI * HF_INJ_FREQ_HZ / CTRL_FREQ_HZ;
    hf->inj_value = 0.0f;
    /* 一阶IIR低通滤波系数: alpha = 2*pi*fc*Ts / (1 + 2*pi*fc*Ts) */
    float rc = 1.0f / (MATH_2PI * LPF_CUTOFF_HZ);
    hf->lpf_alpha = (1.0f / CTRL_FREQ_HZ) / (rc + 1.0f / CTRL_FREQ_HZ);

    hf->id_base_filt = 0.0f;
    hf->iq_base_filt = 0.0f;
    hf->id_hf_sum = 0.0f;
    hf->iq_hf_sum = 0.0f;
    hf->demod_count = 0;
    hf->Ld_hat = 0.001f; /* 初始值：1mH，根据电机实际设置 */
    hf->Lq_hat = 0.001f;
    hf->Ld_filt = hf->Ld_hat;
    hf->Lq_filt = hf->Lq_hat;
    hf->deadtime_comp = 0.0f;
}

/* ---- 死区补偿 (基于电流极性) ---- */
float DeadtimeCompensation(float i_phase, float Vdc, float T_dead, float Ts)
{
    float V_loss = T_dead * Vdc / (2.0f * Ts);
    if (i_phase > 0.1f)
        return V_loss;
    else if (i_phase < -0.1f)
        return -V_loss;
    else
        return 0.0f; /* 零电流钳位区 */
}

/* ---- 主更新函数：每个控制周期调用 ---- */
void HFInjection_Update(HFInjection_t *hf,
                        float id_meas, float iq_meas,
                        float ud_base, float uq_base,
                        float Vdc, float T_dead, float Ts,
                        float i_phase_a)
{
    /* 1. 生成高频注入信号 */
    hf->phase += hf->phase_inc;
    if (hf->phase > MATH_2PI)
        hf->phase -= MATH_2PI;
    hf->inj_value = HF_INJ_AMP_V * sinf(hf->phase);

    /* 2. 死区补偿 */
    hf->deadtime_comp = DeadtimeCompensation(i_phase_a, Vdc, T_dead, Ts);

    /* 3. 低通滤波提取基波电流分量 */
    hf->id_base_filt += hf->lpf_alpha * (id_meas - hf->id_base_filt);
    hf->iq_base_filt += hf->lpf_alpha * (iq_meas - hf->iq_base_filt);

    /* 4. 提取高频电流分量 (原始 - 基波) */
    float id_hf = id_meas - hf->id_base_filt;
    float iq_hf = iq_meas - hf->iq_base_filt;

    /* 5. 同步解调：用注入信号极性提取同相分量 */
    float sign = (hf->inj_value >= 0.0f) ? 1.0f : -1.0f;
    hf->id_hf_sum += id_hf * sign;
    hf->iq_hf_sum += iq_hf * sign;
    hf->demod_count++;

    /* 6. 每完成一个完整注入周期后计算电感 */
    if (hf->demod_count >= (uint16_t)(CTRL_FREQ_HZ / HF_INJ_FREQ_HZ))
    {
        float id_hf_amp = hf->id_hf_sum / (float)hf->demod_count;
        float iq_hf_amp = hf->iq_hf_sum / (float)hf->demod_count;
        float omega_h = MATH_2PI * HF_INJ_FREQ_HZ;

        /* Ld = U_inj / (omega_h * I_dh_amplitude) */
        if (fabsf(id_hf_amp) > 1e-6f)
        {
            hf->Ld_hat = HF_INJ_AMP_V / (omega_h * fabsf(id_hf_amp));
        }
        if (fabsf(iq_hf_amp) > 1e-6f)
        {
            hf->Lq_hat = HF_INJ_AMP_V / (omega_h * fabsf(iq_hf_amp));
        }

        /* 对辨识结果做一阶低通滤波，抑制周期间波动 */
        hf->Ld_filt += 0.1f * (hf->Ld_hat - hf->Ld_filt);
        hf->Lq_filt += 0.1f * (hf->Lq_hat - hf->Lq_filt);

        /* 复位累加器 */
        hf->id_hf_sum = 0.0f;
        hf->iq_hf_sum = 0.0f;
        hf->demod_count = 0;
    }
}

/* ---- 初始化 ---- */
void FFRLS_Init(FFRLS_t *rls,
                float Rs_init, float Psi_f_init,
                float P0, float lambda)
{
    /* 参数初始猜测 */
    rls->theta[0] = Rs_init;    /* Rs */
    rls->theta[1] = Psi_f_init; /* Psi_f */

    /* 协方差矩阵初始化为 alpha * I, alpha 取大值 */
    memset(rls->P, 0, sizeof(rls->P));
    rls->P[0][0] = P0;
    rls->P[1][1] = P0;

    rls->lambda = lambda;
    rls->lambda_min = 0.95f;
    rls->update_count = 0;
}

/* ---- 矩阵辅助运算 (2x2) ---- */
static void MatVec2x2(const float A[RLS_DIM][RLS_DIM],
                      const float x[RLS_DIM],
                      float out[RLS_DIM])
{
    for (int i = 0; i < RLS_DIM; i++)
    {
        out[i] = 0.0f;
        for (int j = 0; j < RLS_DIM; j++)
        {
            out[i] += A[i][j] * x[j];
        }
    }
}

static float VecDot(const float a[RLS_DIM], const float b[RLS_DIM])
{
    float s = 0.0f;
    for (int i = 0; i < RLS_DIM; i++)
        s += a[i] * b[i];
    return s;
}

/* ---- 核心更新：每个控制周期调用 ---- */
void FFRLS_Update(FFRLS_t *rls,
                  float uq, float iq,
                  float id, float omega_e,
                  float Ld_known)
{
    /* 1. 构建回归向量和观测值 */
    /* y = uq - omega_e * Ld * id */
    rls->y = uq - omega_e * Ld_known * id;

    /* phi = [-iq, -omega_e]^T */
    rls->phi[0] = -iq;
    rls->phi[1] = -omega_e;

    /* 2. 计算增益向量 K = P*phi / (lambda + phi^T*P*phi) */
    float P_phi[RLS_DIM];
    MatVec2x2((const float (*)[RLS_DIM])rls->P, rls->phi, P_phi);

    float denom = rls->lambda + VecDot(rls->phi, P_phi);
    if (fabsf(denom) < 1e-12f)
        return; /* 防止除零 */

    for (int i = 0; i < RLS_DIM; i++)
    {
        rls->K[i] = P_phi[i] / denom;
    }

    /* 3. 计算预测误差 */
    float phi_T_theta = VecDot(rls->phi, rls->theta);
    float error = rls->y - phi_T_theta;

    /* 4. 更新参数估计 theta(k) = theta(k-1) + K * error */
    for (int i = 0; i < RLS_DIM; i++)
    {
        rls->theta[i] += rls->K[i] * error;
    }

    /* 5. 更新协方差矩阵 P(k) = (I - K*phi^T) * P(k-1) / lambda */
    float P_new[RLS_DIM][RLS_DIM];
    for (int i = 0; i < RLS_DIM; i++)
    {
        for (int j = 0; j < RLS_DIM; j++)
        {
            float Kphi = rls->K[i] * rls->phi[j];
            if (i == j)
            {
                P_new[i][j] = (1.0f - Kphi) * rls->P[i][j] / rls->lambda;
            }
            else
            {
                P_new[i][j] = -Kphi * rls->P[i][j] / rls->lambda;
            }
        }
    }
    /* 修正：P_new 的对角线应用完整公式 */
    for (int i = 0; i < RLS_DIM; i++)
    {
        for (int j = 0; j < RLS_DIM; j++)
        {
            float sum = 0.0f;
            for (int k = 0; k < RLS_DIM; k++)
            {
                float delta_ik = (i == k) ? 1.0f : 0.0f;
                sum += (delta_ik - rls->K[i] * rls->phi[k]) * rls->P[k][j];
            }
            P_new[i][j] = sum / rls->lambda;
        }
    }
    memcpy(rls->P, P_new, sizeof(rls->P));

    rls->update_count++;

    /* 6. 参数约束：防止物理上不合理的值 */
    if (rls->theta[0] < 0.0f)
        rls->theta[0] = 0.0f; /* Rs >= 0 */
    if (rls->theta[1] < 0.0f)
        rls->theta[1] = 0.0f; /* Psi_f >= 0 */
}

/* ---- 初始化 ---- */
void EKO_Init(EKO_t *eko, float omega_init, float TL_init,
              float J_init, float B_fric, float dt,
              float Q_omega, float Q_TL, float R_meas)
{
    eko->x[0] = omega_init; /* 转速估计初值 */
    eko->x[1] = TL_init;    /* 负载转矩估计初值 */

    /* 协方差矩阵初始化为对角阵 */
    eko->P[0][0] = 0.1f;
    eko->P[0][1] = 0.0f;
    eko->P[1][0] = 0.0f;
    eko->P[1][1] = 0.1f;

    /* 过程噪声协方差 (对角) */
    eko->Q[0][0] = Q_omega;
    eko->Q[0][1] = 0.0f;
    eko->Q[1][0] = 0.0f;
    eko->Q[1][1] = Q_TL;

    eko->R = R_meas;
    eko->J_hat = J_init;
    eko->B = B_fric;
    eko->Te = 0.0f;
    eko->dt = dt;
    eko->q_adapt_gain = 0.01f;
    eko->innovation_prev = 0.0f;
}

/* ---- 主更新函数：每个速度环周期调用 ---- */
void EKO_Update(EKO_t *eko, float omega_meas, float Te)
{
    float dt = eko->dt;

    /* ====== 1. 预测步 (Predict) ====== */
    /* 非线性状态转移:
     * omega(k+1) = omega(k) + dt/J * (Te - TL - B*omega)
     * TL(k+1)    = TL(k)  (假设负载转矩缓变)
     */
    float omega_hat = eko->x[0];
    float TL_hat = eko->x[1];
    float J = eko->J_hat;

    /* 状态预测 */
    float omega_pred = omega_hat + dt / J * (Te - TL_hat - eko->B * omega_hat);
    float TL_pred = TL_hat;

    /* 雅可比矩阵 F = df/dx (2x2)
     * F[0][0] = 1 - dt*B/J
     * F[0][1] = -dt/J
     * F[1][0] = 0
     * F[1][1] = 1
     */
    float F[EKO_STATE_DIM][EKO_STATE_DIM];
    F[0][0] = 1.0f - dt * eko->B / J;
    F[0][1] = -dt / J;
    F[1][0] = 0.0f;
    F[1][1] = 1.0f;

    /* 协方差预测: P_pred = F*P*F^T + Q */
    float FP[EKO_STATE_DIM][EKO_STATE_DIM];
    for (int i = 0; i < EKO_STATE_DIM; i++)
    {
        for (int j = 0; j < EKO_STATE_DIM; j++)
        {
            FP[i][j] = 0.0f;
            for (int k = 0; k < EKO_STATE_DIM; k++)
            {
                FP[i][j] += F[i][k] * eko->P[k][j];
            }
        }
    }

    float P_pred[EKO_STATE_DIM][EKO_STATE_DIM];
    for (int i = 0; i < EKO_STATE_DIM; i++)
    {
        for (int j = 0; j < EKO_STATE_DIM; j++)
        {
            P_pred[i][j] = eko->Q[i][j];
            for (int k = 0; k < EKO_STATE_DIM; k++)
            {
                P_pred[i][j] += FP[i][k] * F[j][k]; /* F^T 对应 F[j][k] */
            }
        }
    }

    /* ====== 2. 自适应Q矩阵调整 ====== */
    /* 根据新息大小动态调整Q，新息大表明工况变化 */
    float innovation = omega_meas - omega_pred;
    float innov_abs = fabsf(innovation);

    if (innov_abs > 0.5f * fabsf(eko->innovation_prev) &&
        eko->innovation_prev != 0.0f)
    {
        /* 工况变化明显，增大Q以加快响应 */
        eko->Q[0][0] *= (1.0f + eko->q_adapt_gain);
        eko->Q[1][1] *= (1.0f + eko->q_adapt_gain);
    }
    else
    {
        /* 稳态，逐渐恢复Q到基础值 */
        eko->Q[0][0] *= (1.0f - 0.1f * eko->q_adapt_gain);
        eko->Q[1][1] *= (1.0f - 0.1f * eko->q_adapt_gain);
    }

    /* Q矩阵限幅 */
    if (eko->Q[0][0] > 10.0f)
        eko->Q[0][0] = 10.0f;
    if (eko->Q[1][1] > 100.0f)
        eko->Q[1][1] = 100.0f;
    if (eko->Q[0][0] < 1e-6f)
        eko->Q[0][0] = 1e-6f;
    if (eko->Q[1][1] < 1e-6f)
        eko->Q[1][1] = 1e-6f;

    eko->innovation_prev = innovation;

    /* 用调整后的Q重新计算P_pred */
    P_pred[0][0] += (eko->Q[0][0] - eko->Q[0][0]); /* 已在上面加入Q，此处仅示意 */
    /* 实际实现中应将自适应Q在预测步之前完成更新 */

    /* ====== 3. 更新步 (Update) ====== */
    /* 测量方程: z = H*x + v, H = [1, 0] */
    float H[EKO_STATE_DIM] = {1.0f, 0.0f};

    /* 新息协方差 S = H*P_pred*H^T + R */
    float S = eko->R;
    for (int i = 0; i < EKO_STATE_DIM; i++)
    {
        for (int j = 0; j < EKO_STATE_DIM; j++)
        {
            S += H[i] * P_pred[i][j] * H[j];
        }
    }

    /* 卡尔曼增益 K = P_pred * H^T / S */
    float K[EKO_STATE_DIM];
    for (int i = 0; i < EKO_STATE_DIM; i++)
    {
        float PH = 0.0f;
        for (int j = 0; j < EKO_STATE_DIM; j++)
        {
            PH += P_pred[i][j] * H[j];
        }
        K[i] = PH / S;
    }

    /* 状态更新 */
    eko->x[0] = omega_pred + K[0] * innovation;
    eko->x[1] = TL_pred + K[1] * innovation;

    /* 协方差更新: P = (I - K*H) * P_pred */
    for (int i = 0; i < EKO_STATE_DIM; i++)
    {
        for (int j = 0; j < EKO_STATE_DIM; j++)
        {
            float sum = 0.0f;
            for (int k = 0; k < EKO_STATE_DIM; k++)
            {
                float delta_ik = (i == k) ? 1.0f : 0.0f;
                sum += (delta_ik - K[i] * H[k]) * P_pred[k][j];
            }
            eko->P[i][j] = sum;
        }
    }

    /* 存储电磁转矩供参考 */
    eko->Te = Te;
}

/* ---- 初始化 ---- */
void MRAS_Init(MRAS_t *mras, float J_init, float gamma,
               float dt, float omega_init)
{
    mras->omega_adj = omega_init;
    mras->J_hat = J_init;
    mras->omega_ref = omega_init;
    mras->omega_meas = omega_init;
    mras->gamma = gamma;
    mras->gamma_min = MRAS_GAIN_MIN;
    mras->gamma_max = MRAS_GAIN_MAX;
    mras->integral_term = 0.0f;
    mras->Te = 0.0f;
    mras->TL_hat = 0.0f;
    mras->dt = dt;
    mras->J_filt = J_init;
    mras->output_lpf = 0.01f; /* 输出滤波系数，越小越平滑但滞后越大 */
    mras->error = 0.0f;
    mras->error_prev = 0.0f;
    mras->converge_count = 0;
    mras->converged = 0;
}

/* ---- 主更新函数：每个速度环周期调用 ---- */
void MRAS_Update(MRAS_t *mras,
                 float omega_meas,
                 float Te,
                 float TL_hat)
{
    float dt = mras->dt;
    mras->omega_meas = omega_meas;
    mras->Te = Te;
    mras->TL_hat = TL_hat;

    /* ====== 1. 计算参考模型输出 ======
     * 参考模型直接使用实际测量的转速作为参考
     */
    mras->omega_ref = omega_meas;

    /* ====== 2. 可调模型：用 J_hat 重构转速 ======
     * omega_adj(k+1) = omega_adj(k) + dt/J_hat * (Te - TL_hat)
     */
    float J = mras->J_hat;
    if (J < 1e-6f)
        J = 1e-6f; /* 防止除零 */

    mras->omega_adj += dt / J * (Te - TL_hat);

    /* ====== 3. 计算跟踪误差 ====== */
    mras->error = mras->omega_ref - mras->omega_adj;

    /* ====== 4. 变增益策略 ======
     * 根据误差大小动态调整自适应增益：
     * 误差大时增大增益加快收敛，误差小时减小增益抑制振荡
     */
    float error_abs = fabsf(mras->error);
    float gamma_adaptive;

    if (error_abs > 0.1f)
    {
        /* 大误差：增大增益 */
        gamma_adaptive = mras->gamma * (1.0f + 2.0f * error_abs);
    }
    else if (error_abs > 0.01f)
    {
        /* 中等误差：保持基本增益 */
        gamma_adaptive = mras->gamma;
    }
    else
    {
        /* 小误差：减小增益，提高稳态精度 */
        gamma_adaptive = mras->gamma * 0.5f;
    }

    /* 增益限幅 */
    if (gamma_adaptive > mras->gamma_max)
        gamma_adaptive = mras->gamma_max;
    if (gamma_adaptive < mras->gamma_min)
        gamma_adaptive = mras->gamma_min;

    /* ====== 5. 自适应律：更新 J_hat ======
     * 基于Lyapunov稳定性理论推导的自适应律：
     * d(1/J_hat)/dt = -gamma * error * (Te - TL_hat) / J_hat
     * 等价于:
     * d(J_hat)/dt = gamma * error * (Te - TL_hat)
     *
     * 离散化实现:
     * J_hat(k+1) = J_hat(k) + gamma * dt * error * (Te - TL_hat)
     */
    float driving_term = Te - TL_hat;
    float J_dot = gamma_adaptive * mras->error * driving_term;

    /* 积分更新，带约束防止发散 */
    float J_new = mras->J_hat + dt * J_dot;

    /* 物理约束：转动惯量必须为正且在一定范围内 */
    if (J_new > 1e-4f && J_new < 100.0f)
    {
        mras->J_hat = J_new;
    }

    /* ====== 6. 输出滤波 ====== */
    mras->J_filt += mras->output_lpf * (mras->J_hat - mras->J_filt);

    /* ====== 7. 收敛检测 ====== */
    if (fabsf(mras->error) < 0.001f &&
        fabsf(mras->error - mras->error_prev) < 1e-5f)
    {
        mras->converge_count++;
        if (mras->converge_count > 100)
        {
            mras->converged = 1;
        }
    }
    else
    {
        mras->converge_count = 0;
        mras->converged = 0;
    }

    mras->error_prev = mras->error;
}
