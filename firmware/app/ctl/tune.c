#include "tune.h"
#include "math_fast.h"
#include "bsp_adc.h"

#include "device.h"
#include "data_manager.h"
#include "filter.h"
// ================================= 全局变量定义 =================================
static tTuneParams temp_params = {0};
static tTuneContext tune_ctx = {0};

static tFirstOrderLagFilter rs_i_filter;
// ================================= 辅助函数 =================================
// sum型线性拟合
static void _fit_from_sums(float sum_x, float sum_y, float sum_xy, float sum_xx, u16 n,
                           float *k, float *b, float *mse)
{
    if (n < 10)
    {
        *k = 0;
        *b = 0;
        *mse = 1e9f;
        return;
    }

    float denom = n * sum_xx - sum_x * sum_x;
    if (fabsf(denom) < 1e-6f)
    {
        *k = 0;
        *b = 0;
        *mse = 1e9f;
        return;
    }

    *k = (n * sum_xy - sum_x * sum_y) / denom;
    *b = (sum_y - (*k) * sum_x) / n;

    // 估算均方误差（简化版，避免存储残差）
    // mse ≈ (Σy² - 2k·Σxy - 2b·Σy + k²·Σxx + 2kb·Σx + b²·n) / n
    // 为简化，此处返回 0，实际调试时可补充 Σy² 累加
    *mse = 0.0f;
}
// 控制带宽、滤波系数与PID参数的经验公式
static void calculate_control_params()
{
    // TODO:添加电流环软硬调节
    float fn_d = 1 / (MATH_2PI * temp_params.ld / temp_params.rs);
    float wc_d = MATH_2PI * 0.5f * (2 * fn_d < F_CURRENT / 10 ? 2 * fn_d : F_CURRENT / 10);
    temp_params.id_kp = wc_d * temp_params.ld;
    temp_params.id_ki = wc_d * temp_params.rs;

    float fn_q = 1 / (MATH_2PI * temp_params.lq / temp_params.rs);
    float wc_q = MATH_2PI * 0.6f * (2 * fn_q < F_CURRENT / 10 ? 2 * fn_q : F_CURRENT / 10);
    temp_params.iq_kp = wc_q * temp_params.lq;
    temp_params.iq_ki = wc_q * temp_params.rs;

    // 取电流环开环截止频率 (Hz)
    float f_c_open = fminf(wc_d, wc_q) / MATH_2PI;

    // 设计滤波截止频率：取开环截止频率的 4倍
    float f_filter = 4.0f * f_c_open;

    // 上下限约束
    float f_sw = (float)F_PWM;     // ADC采样频率 = PWM频率
    float f_max = f_sw / 8.0f;     // 最大允许滤波截止频率（保证滤除开关纹波）
    float f_min = 2.0f * f_c_open; // 最小允许值（避免影响环路）

    if (f_filter > f_max)
        f_filter = f_max;
    if (f_filter < f_min)
        f_filter = f_min;

    // 计算一阶低通滤波系数 alpha
    temp_params.cur_filter_alpha = 1.0f - expf(-MATH_2PI * f_filter / f_sw);

    // 直接写入
    foc_set_cur_loop_param(temp_params.id_kp, temp_params.id_ki, temp_params.iq_kp, temp_params.iq_ki);
}

// 累加当前采样点对DFT的贡献（无缓冲区）
// i_alpha    α轴电流
// i_beta     β轴电流
// omega_dt   ω_inj * dt (rad) 每次固定角度增量
// sum_re     实部累加器指针
// sum_im     虚部累加器指针
// cnt        当前周期内已采样点数指针（从0开始递增）
static void dft_accumulate(float i_alpha, float i_beta, float omega_dt,
                           float *sum_re, float *sum_im, uint16_t *cnt)
{
    float angle = omega_dt * (*cnt); // θ = ωt
    float c = cosf(angle);
    float s = -sinf(angle); // e^{-jθ}
    // 复数乘法: (i_alpha + j i_beta) * (c + j s) 的实部和虚部
    // 注意：实际需要的 DFT 系数是 e^{-jθ}，即 (c + j s) 其中 s = -sin(θ)
    *sum_re += i_alpha * c - i_beta * s;
    *sum_im += i_alpha * s + i_beta * c;
    (*cnt)++;
}
// 根据累加和计算电流幅值
// sum_re   实部累加和
// sum_im   虚部累加和
// samples  该周期内的采样点数（恒为 SAMPLES_PER_CYCLE）
// 电流基波幅值 (A)
static float dft_get_amplitude(float sum_re, float sum_im, uint16_t samples)
{
    float scale = 2.0f / samples;
    float re = sum_re * scale;
    float im = sum_im * scale;
    return sqrtf(re * re + im * im);
}
static void dft_reset_accumulator(float *sum_re, float *sum_im, uint16_t *cnt)
{
    *sum_re = 0.0f;
    *sum_im = 0.0f;
    *cnt = 0;
}

// ================================= 参数访问实现 =================================
void motor_param_tune_force_save(void)
{
    //  这里是集中写入参数flash中转站
    g_Param.motor_rs = temp_params.rs;
    g_Param.motor_ld = temp_params.ld;
    g_Param.motor_lq = temp_params.lq;
    g_Param.motor_psif = temp_params.psi_f;
    g_Param.motor_ke = temp_params.ke;
    g_Param.motor_j = temp_params.j;
    g_Param.motor_b = temp_params.b;
    g_Param.motor_polepairs = temp_params.pole_pairs;
    g_Param.theta_offset = temp_params.theta_offset;
    g_Param.theta_elec_offset = temp_params.theta_elec_need_180;
    g_Param.forward_dir = temp_params.direction;

    g_Param.kp_Q = temp_params.iq_kp;
    g_Param.ki_Q = temp_params.iq_ki;
    g_Param.kp_D = temp_params.id_kp;
    g_Param.ki_D = temp_params.id_ki;

    g_Param.cur_filter_alpha = temp_params.cur_filter_alpha;
    g_Param.adc_U_zero_offset = temp_params.uadc_offset;
    g_Param.adc_V_zero_offset = temp_params.vadc_offset;
    g_Param.adc_W_zero_offset = temp_params.wadc_offset;
}

// ================================= 初始化与重置 =================================
// todo:后续添加无感整定，根据选择的感应模式校准，无感只校准电机，有感校准电机和编码器，直接校准电机，直接用HFI、SMO做精准的控制
void motor_param_tune_init()
{
    memset(&tune_ctx, 0, sizeof(tTuneContext));
    memset(&temp_params, 0, sizeof(tTuneParams));
    temp_params.kv = g_Param.motor_kv;
    temp_params.dt = T_CON;
    temp_params.tune_cur_limit = g_Param.tune_current; // 从用户配置获取校准电流锚点
    filter_first_order_lag_init(&rs_i_filter, 0.04f, 0);
}

void motor_param_tune_reset()
{
    tune_ctx.fault = FAULT_NONE;
    tune_ctx.state = TUNE_INIT;
}
// ================================= 电阻整定 (开环 + 滤波 + 差分) =================================

// 开环电压测电阻
static bool _tune_rs_ol_vol(float i_alpha)
{
    tTuneContext *ctx = &tune_ctx;
    float i_a = filter_first_order_lag(&rs_i_filter, i_alpha);

    if (ctx->freq_tick++ < RS_FREQ_F)
        return false; // 跳过不整定
    ctx->freq_tick = 0;

    //===滞环电流控制=== (阈值在 TUNE_IDLE 时已预计算至 rs_ctx)
    float err = ctx->rs_ctx.i_target - i_a;
    if (FABSF(err) > ctx->rs_ctx.hyst_band)
    {
        ctx->rs_ctx.v_cmd += (err > 0) ? 10 * RS_V_STEP_MIN : -10 * RS_V_STEP_MIN;
        ctx->rs_ctx.v_cmd = CLAMP(ctx->rs_ctx.v_cmd, -ctx->rs_ctx.v_limit, ctx->rs_ctx.v_limit);
        ctx->rs_ctx.hold_cnt = 0;
    }
    else
    {
        if (++ctx->rs_ctx.hold_cnt >= RS_V_HOLD_MAX_TICKS)
        {
            ctx->rs_ctx.v_cmd += (err > 0) ? RS_V_STEP_MIN : -RS_V_STEP_MIN;
            ctx->rs_ctx.v_cmd = CLAMP(ctx->rs_ctx.v_cmd, -ctx->rs_ctx.v_limit, ctx->rs_ctx.v_limit);
            ctx->rs_ctx.hold_cnt = 0;
        }
    }
    //===死区前馈===
    float v_dead_comp = (i_a > 0) ? RS_DEADTIME_VCOMP : -RS_DEADTIME_VCOMP;
    float v_out_ab[2] = {0};
    v_out_ab[0] = CLAMP(ctx->rs_ctx.v_cmd + v_dead_comp, -ctx->rs_ctx.v_limit, ctx->rs_ctx.v_limit);
    v_out_ab[1] = 0;

    // 稳态判断
    float i_err = FABSF(i_a - ctx->rs_ctx.i_target);
    if (i_err < ctx->rs_ctx.steady_err)
    {
        if (ctx->steady_tick >= RS_STEADY_TICKS)
        {
            // === 4. 数据采集 ===
            if (ctx->rs_ctx.step == 0)
            {
                // 记录第一点
                ctx->rs_ctx.i_meas[0] = i_a;
                ctx->rs_ctx.v_meas[0] = v_out_ab[0]; // 记录实际输出电压

                // 切换到第二点
                ctx->rs_ctx.step = 1;
                ctx->steady_tick = 0;
                ctx->rs_ctx.step_ticks = 0;

                // 切换到第二点 (目标电流 = tune_cur_limit × 系数)
                ctx->rs_ctx.i_target = ctx->rs_ctx.i_target_2;
                ctx->rs_ctx.v_cmd = 0;
                ctx->rs_ctx.hold_cnt = 0;

                return false; // 继续第二点
            }
            else
            {
                // 记录第二点
                ctx->rs_ctx.i_meas[1] = i_a;
                ctx->rs_ctx.v_meas[1] = v_out_ab[0]; // 记录实际输出电压

                // === 5. 差分计算电阻 ===
                float delta_i = ctx->rs_ctx.i_meas[1] - ctx->rs_ctx.i_meas[0];
                float delta_v = ctx->rs_ctx.v_meas[1] - ctx->rs_ctx.v_meas[0];

                // 信噪比检查 (阈值 = tune_cur_limit × 系数)
                if (FABSF(delta_i) < ctx->rs_ctx.min_delta_i)
                {
                    ctx->fault = FAULT_TUNE_CURRENT_VIBRATION;
                    return true;
                }

                // 计算电阻
                temp_params.rs = delta_v / (delta_i + 1e-6f);

                // 合理性校验
                if (temp_params.rs < RS_RANGE_MIN || temp_params.rs > RS_RANGE_MAX)
                {
                    ctx->fault = FAULT_RS_LS_CAL_FAIL;
                    return true;
                }
                v_out_ab[0] = 0;
                v_out_ab[1] = 0;
                foc_set_target(v_out_ab);
                return true; // 辨识完成
            }
        }
    }
    else
    {
        // 电流波动，重置稳态计数
        ctx->steady_tick = 0;
    }

    // === 6. 输出电压 ===

    foc_set_target(v_out_ab);
    return false; // 辨识进行中
}
// 开环电流测三相电阻 (双点差分 × 3角度)
// 在 OPEN_CUR 模式下，施加固定电角度 + q 轴电流，用 αβ 电压幅值计算 Rs
// 三个电角度 (270°, 30°, 150°) 各 120° 间隔，分别以 U / V / W 相为主载流相
// 每个角度做两点差分：I₁ = tune_cur × 0.2, I₂ = tune_cur × 0.6
static bool _tune_rs_ol_cur(tFOC_val *foc_val)
{
    tTuneContext *ctx = &tune_ctx;

    // 三个电角度 [rad]，各隔 120° (2.094 rad)，分别使 U/V/W 相为主载流相
    static const float angles[3] = {4.7124f, 0.5236f, 2.6180f};

    // ol_stage: 0=I₁@U, 1=I₂@U; 2=I₁@V, 3=I₂@V; 4=I₁@W, 5=I₂@W; 6=done
    u8 stage = ctx->rs_ctx.ol_stage;

    if (stage >= 6)
    {
        float rs_sum = ctx->rs_ctx.ol_rs[0] + ctx->rs_ctx.ol_rs[1] + ctx->rs_ctx.ol_rs[2];
        temp_params.rs = rs_sum / 3.0f;

        if (temp_params.rs < RS_RANGE_MIN || temp_params.rs > RS_RANGE_MAX)
            ctx->fault = FAULT_RS_LS_CAL_FAIL;

        float v_zero[2] = {0, 0};
        foc_set_target(v_zero);
        ctx->rs_ctx.ol_stage = 0;
        return true;
    }

    u8 angle_idx = stage / 2; // 0,1,2
    u8 cur_idx = stage % 2;   // 0=I₁, 1=I₂
    float cur_lim = temp_params.tune_cur_limit;

    float i_target = (cur_idx == 0) ? (cur_lim * RS_I_TARGET_1_COEF)
                                    : (cur_lim * RS_I_TARGET_2_COEF);
    float theta = angles[angle_idx];

    foc_set_ol_theta_cur(theta, i_target);

    // 稳态判断：实际电流 vs 参考电流
    float i_err_a = FABSF(foc_val->ialpha - foc_val->ialpha_ref);
    float i_err_b = FABSF(foc_val->ibeta - foc_val->ibeta_ref);
    float i_err = (i_err_a > i_err_b) ? i_err_a : i_err_b;
    float steady_thr = cur_lim * RS_STEADY_ERR_THR_COEF;

    if (i_err < steady_thr)
    {
        if (ctx->steady_tick >= RS_STEADY_TICKS)
        {
            float u_mag = sqrtf(foc_val->ualpha * foc_val->ualpha + foc_val->ubeta * foc_val->ubeta);

            if (cur_idx == 0)
            {
                ctx->rs_ctx.ol_u[0] = u_mag;
                ctx->rs_ctx.ol_stage++;
            }
            else
            {
                ctx->rs_ctx.ol_u[1] = u_mag;

                float I1 = cur_lim * RS_I_TARGET_1_COEF;
                float I2 = cur_lim * RS_I_TARGET_2_COEF;
                float delta_i = I2 - I1;
                float delta_u = ctx->rs_ctx.ol_u[1] - ctx->rs_ctx.ol_u[0];

                if (FABSF(delta_i) < cur_lim * RS_MIN_DELTA_I_COEF)
                {
                    ctx->fault = FAULT_TUNE_CURRENT_VIBRATION;
                    return true;
                }

                ctx->rs_ctx.ol_rs[angle_idx] = delta_u / (delta_i + 1e-6f);
                ctx->rs_ctx.ol_stage++;
            }
            ctx->steady_tick = 0;
        }
    }
    else
    {
        ctx->steady_tick = 0;
    }

    return false;
}
// ================================= 电感整定（alpha beta轴方波注入） =================================

static bool _TuneLs(float v_alpha, float v_beta, float i_alpha, float i_beta)
{

    tTuneContext *ctx = &tune_ctx;
    const float omega_inj = MATH_2PI * LS_INJECT_FREQ_HZ; // 注入角频率 (rad/s)
    const float omega_dt = omega_inj * temp_params.dt;    // 每步相位增量 (°)

    static float inj_angle = 0.0f; // 当前注入电压相位
    float v_cmd_ab[2] = {0};

    // 为了代码可读性，将上下文指针暂存
    float *sum_re = &ctx->ls_ctx.sum_re;
    float *sum_im = &ctx->ls_ctx.sum_im;
    uint16_t *sample_cnt = &ctx->ls_ctx.sample_cnt;

    switch (ctx->ls_ctx.state)
    {
    // ==================== 状态0：自适应注入电压 ====================
    case 0:
        // 初始化自适应过程 (v_inj 已在状态机入口计算好)
        if (!ctx->ls_ctx.ready)
        {
            ctx->ls_ctx.i_target = temp_params.tune_cur_limit * LS_I_TARGET_COEF;
            dft_reset_accumulator(sum_re, sum_im, sample_cnt);
            ctx->ls_ctx.amp_sum = 0.0f;
            ctx->ls_ctx.cycle_cnt = 0;
            inj_angle = 0.0f;
            ctx->ls_ctx.ready = true;
        }

        // 生成旋转电压 (αβ 旋转，使电流幅值与转子位置无关)
        v_cmd_ab[0] = ctx->ls_ctx.v_inj * arm_cos_f32(inj_angle);
        v_cmd_ab[1] = ctx->ls_ctx.v_inj * arm_sin_f32(inj_angle);
        foc_set_target(v_cmd_ab);

        // 更新注入相位
        inj_angle += omega_dt;
        if (inj_angle > MATH_2PI)
            inj_angle -= MATH_2PI;

        // DFT 累加当前的电流值
        dft_accumulate(i_alpha, i_beta, omega_dt, sum_re, sum_im, sample_cnt);

        // 完成一个注入周期
        if (*sample_cnt >= LS_INJECT_FREQ_TICK)
        {
            // 计算这个周期的电流幅值
            float I_mag = dft_get_amplitude(*sum_re, *sum_im, LS_INJECT_FREQ_TICK);
            ctx->ls_ctx.amp_sum += I_mag;
            ctx->ls_ctx.cycle_cnt++;

            // 重置累加器，准备下一个周期
            dft_reset_accumulator(sum_re, sum_im, sample_cnt);

            if (ctx->ls_ctx.cycle_cnt >= DFT_AVG_CYCLES)
            {
                float I_avg = ctx->ls_ctx.amp_sum / ctx->ls_ctx.cycle_cnt;
                ctx->ls_ctx.i_meas = I_avg;

                // 判断电流是否在目标范围 [i_target × (1-hyst), i_target × (1+hyst)]
                if (I_avg < ctx->ls_ctx.i_hyst_lo && ctx->ls_ctx.v_inj < ctx->ls_ctx.v_max - ctx->ls_ctx.v_step)
                {
                    ctx->ls_ctx.v_inj += ctx->ls_ctx.v_step;
                    if (ctx->ls_ctx.v_inj > ctx->ls_ctx.v_max)
                        ctx->ls_ctx.v_inj = ctx->ls_ctx.v_max;
                }
                else if (I_avg > ctx->ls_ctx.i_hyst_hi && ctx->ls_ctx.v_inj > ctx->ls_ctx.v_step)
                {
                    ctx->ls_ctx.v_inj -= ctx->ls_ctx.v_step;
                    if (ctx->ls_ctx.v_inj < 0.1f)
                        ctx->ls_ctx.v_inj = 0.1f;
                }
                else
                {
                    // 电压已合适，结束自适应阶段
                    ctx->ls_ctx.state = 1; // 进入转子预定位
                    ctx->steady_tick = 0;  // 重置定时器
                    ctx->ls_ctx.cycle_cnt = 0;
                    v_cmd_ab[0] = 0;
                    v_cmd_ab[1] = 0;
                    foc_set_target(v_cmd_ab);
                    return false;
                }

                // 重置平均累加器，继续下一轮自适应
                ctx->ls_ctx.amp_sum = 0.0f;
                ctx->ls_ctx.cycle_cnt = 0;
            }
        }
        return false; // 尚未完成

    // ==================== 状态1：转子预定位（拉至0°电角度） ====================
    // 缓慢对齐：电压从0斜坡上升至 v_inj，避免过冲
    case 1:
    {
        // 斜坡因子 [0.0 ~ 1.0)
        float ramp = (float)ctx->steady_tick / (float)ctx->align_total_ticks;
        if (ramp > 1.0f)
            ramp = 1.0f;
        float v_align = ctx->ls_ctx.v_inj * ramp;
        v_cmd_ab[0] = v_align;
        v_cmd_ab[1] = 0.0f;
        foc_set_target(v_cmd_ab);

        if (ctx->steady_tick > ctx->align_total_ticks)
        {
            // 对齐完成，断电
            v_cmd_ab[0] = 0;
            v_cmd_ab[1] = 0;
            foc_set_target(v_cmd_ab);
            // 等待电流衰减后再进入测量
            if (ctx->steady_tick > ctx->align_total_ticks + MS_TO_TICK(WAIT_AFTER_ALIGN_MS))
            {
                ctx->ls_ctx.state = 2;
                ctx->steady_tick = 0;

                // 初始化 DFT 测量 Ld
                dft_reset_accumulator(sum_re, sum_im, sample_cnt);
                ctx->ls_ctx.amp_sum = 0.0f;
                ctx->ls_ctx.cycle_cnt = 0;
                inj_angle = 0.0f;
            }
        }
        return false;
    }

    // ==================== 状态2：测量 ld ====================
    case 2:

        // 注入正弦电压（仅α轴，因转子已在0°，α轴对应d轴）

        v_cmd_ab[0] = ctx->ls_ctx.v_inj * arm_cos_f32(inj_angle);
        v_cmd_ab[1] = 0.0f;
        foc_set_target(v_cmd_ab);

        inj_angle += omega_dt;
        if (inj_angle > MATH_2PI)
            inj_angle -= MATH_2PI;

        dft_accumulate(i_alpha, i_beta, omega_dt, sum_re, sum_im, sample_cnt);

        if (*sample_cnt >= LS_INJECT_FREQ_TICK)
        {
            float I_mag = dft_get_amplitude(*sum_re, *sum_im, LS_INJECT_FREQ_TICK);
            ctx->ls_ctx.amp_sum += I_mag;
            ctx->ls_ctx.cycle_cnt++;
            dft_reset_accumulator(sum_re, sum_im, sample_cnt);

            if (ctx->ls_ctx.cycle_cnt >= DFT_AVG_CYCLES)
            {
                float I_avg = ctx->ls_ctx.amp_sum / ctx->ls_ctx.cycle_cnt;
                // 电阻补偿：L = sqrt( (V/R)^2? 正确公式: L = sqrt( (V/I)^2 - Rs^2 ) / ω
                float V_rms = ctx->ls_ctx.v_inj * MATH_1_SQRT2; // 注入电压峰值 -> 有效值
                float I_rms = I_avg * MATH_1_SQRT2;
                float Z = V_rms / I_rms; // 阻抗模
                float L = 0.0f;
                if (Z * Z > temp_params.rs * temp_params.rs)
                {
                    arm_sqrt_f32(Z * Z - temp_params.rs * temp_params.rs, &L);
                    L = L / omega_inj;
                }
                else
                {
                    L = 0.0f; // 不合理，置0
                }
                temp_params.ld = L;
                // 测量完成，进入状态3：旋转转子至90°
                ctx->ls_ctx.state = 3;
                ctx->steady_tick = 0;
                v_cmd_ab[0] = 0;
                v_cmd_ab[1] = 0;
                foc_set_target(v_cmd_ab);
            }
        }
        return false;

    // ==================== 状态3：旋转转子至90°电角度（准备测Lq） ====================
    // 缓慢对齐：电压从0斜坡上升至 v_inj，避免过冲
    case 3:
    {
        // 斜坡因子 [0.0 ~ 1.0)
        float ramp = (float)ctx->steady_tick / (float)ctx->align_total_ticks;
        if (ramp > 1.0f)
            ramp = 1.0f;
        float v_align = ctx->ls_ctx.v_inj * ramp;
        // 施加β轴直流电压，使转子从0°转到90°电角度
        v_cmd_ab[0] = 0.0f;
        v_cmd_ab[1] = v_align;
        foc_set_target(v_cmd_ab);

        if (ctx->steady_tick > ctx->align_total_ticks)
        {
            // 对齐完成，断电
            v_cmd_ab[0] = 0;
            v_cmd_ab[1] = 0;
            foc_set_target(v_cmd_ab);
            // 等待电流衰减后再进入测量
            if (ctx->steady_tick > ctx->align_total_ticks + MS_TO_TICK(WAIT_AFTER_ALIGN_MS))
            {
                ctx->ls_ctx.state = 4;
                ctx->steady_tick = 0;

                // 初始化 DFT 测量 Lq
                dft_reset_accumulator(sum_re, sum_im, sample_cnt);
                ctx->ls_ctx.amp_sum = 0.0f;
                ctx->ls_ctx.cycle_cnt = 0;
                inj_angle = 0.0f;
            }
        }
        return false;
    }

    // ==================== 状态4：测量 lq ====================
    case 4:

        // 依旧在α轴注入正弦（此时转子90°，α轴对准q轴）
        v_cmd_ab[0] = ctx->ls_ctx.v_inj * arm_cos_f32(inj_angle);
        v_cmd_ab[1] = 0.0f;
        foc_set_target(v_cmd_ab);

        inj_angle += omega_dt;
        if (inj_angle > MATH_2PI)
            inj_angle -= MATH_2PI;

        dft_accumulate(i_alpha, i_beta, omega_dt, sum_re, sum_im, sample_cnt);

        if (*sample_cnt >= LS_INJECT_FREQ_TICK)
        {
            float I_mag = dft_get_amplitude(*sum_re, *sum_im, LS_INJECT_FREQ_TICK);
            ctx->ls_ctx.amp_sum += I_mag;
            ctx->ls_ctx.cycle_cnt++;
            dft_reset_accumulator(sum_re, sum_im, sample_cnt);

            if (ctx->ls_ctx.cycle_cnt >= DFT_AVG_CYCLES)
            {
                float I_avg = ctx->ls_ctx.amp_sum / ctx->ls_ctx.cycle_cnt;
                float V_rms = ctx->ls_ctx.v_inj * MATH_1_SQRT2;
                float I_rms = I_avg * MATH_1_SQRT2;
                float Z = V_rms / I_rms;
                float L = 0.0f;
                if (Z * Z > temp_params.rs * temp_params.rs)
                {
                    arm_sqrt_f32(Z * Z - temp_params.rs * temp_params.rs, &L);
                    L = L / omega_inj;
                }
                temp_params.lq = L;
                // 测量完成，进入状态5
                ctx->ls_ctx.state = 5;
                v_cmd_ab[0] = 0;
                v_cmd_ab[1] = 0;
                foc_set_target(v_cmd_ab);
            }
        }
        return false;

    // ==================== 状态5：完成，保存结果 ====================
    case 5:
        // 可选：对结果进行合理性检查（例如范围 0.01mH ~ 100mH）
        if (temp_params.ld < LS_RANGE_MIN || temp_params.ld > LS_RANGE_MAX)
            ctx->fault = FAULT_RS_LS_CAL_FAIL;
        if (temp_params.lq < LS_RANGE_MIN || temp_params.lq > LS_RANGE_MAX)
            ctx->fault = FAULT_RS_LS_CAL_FAIL;
        return true; // 校准完成

    default:
        return true;
    }

    return false; // 未完成
}

// 编码器校准 — 使用 OPEN_CUR 模式
// TODO:把180反转直接计算进offset
bool _tune_encoder(float theta_m)
{
    tTuneContext *ctx = &tune_ctx;
    if (ctx->freq_tick++ < EC_FREQ_F)
        return false;
    ctx->freq_tick = 0;
    float cur_lim = temp_params.tune_cur_limit;

    switch (ctx->encoder_ctx.step)
    {
    case 0:
        ctx->encoder_ctx.theta_m_unwrap = theta_m + MATH_2PI * encoder_get_num_turns();
        switch (ctx->encoder_ctx.test_step)
        {
        case 0:
            foc_set_ol_theta_cur(4.7124f, ctx->encoder_ctx.cur_test);
            if (ctx->steady_tick >= EC_ALIGN_ms)
            {
                ctx->steady_tick = 0;
                ctx->encoder_ctx.theta_m_start = ctx->encoder_ctx.theta_m_unwrap;
                ctx->encoder_ctx.test_step = 1;
            }
            break;
        case 1:
            foc_set_ol_theta_cur(1.5708f, ctx->encoder_ctx.cur_test);
            if (ctx->steady_tick >= EC_ALIGN_ms)
            {
                ctx->steady_tick = 0;
                ctx->encoder_ctx.delta_theta[0] = FABSF(ctx->encoder_ctx.theta_m_unwrap - ctx->encoder_ctx.theta_m_start);
                ctx->encoder_ctx.theta_m_start = ctx->encoder_ctx.theta_m_unwrap;
                ctx->encoder_ctx.test_step = 2;
            }
            break;
        case 2:
            foc_set_ol_theta_cur(4.7124f, ctx->encoder_ctx.cur_test);
            if (ctx->steady_tick >= EC_ALIGN_ms)
            {
                ctx->steady_tick = 0;
                ctx->encoder_ctx.delta_theta[1] = FABSF(ctx->encoder_ctx.theta_m_unwrap - ctx->encoder_ctx.theta_m_start);
                ctx->encoder_ctx.theta_m_start = ctx->encoder_ctx.theta_m_unwrap;
                ctx->encoder_ctx.test_step = 3;
            }
            break;
        case 3:
            if (ctx->encoder_ctx.delta_theta[0] < 0.1745f || ctx->encoder_ctx.delta_theta[1] < 0.1745f)
            {
                ctx->encoder_ctx.cur_test += cur_lim * 0.05f;
                if (ctx->encoder_ctx.cur_test > cur_lim * 0.3f)
                {
                    ctx->fault = FAULT_MOTOR_LOCK;
                    ctx->state = TUNE_FAILED;
                    return true;
                }
                ctx->encoder_ctx.test_step = 0;
                break;
            }
            ctx->encoder_ctx.test_step = 0;
            ctx->encoder_ctx.step = 1;
            break;
        }
        break;

    case 1:
        foc_set_ol_theta_cur(4.7124f, ctx->encoder_ctx.cur_test);
        if (ctx->steady_tick >= EC_ALIGN_ms)
        {
            ctx->steady_tick = 0;
            ctx->encoder_ctx.theta_m_unwrap = theta_m + MATH_2PI * encoder_get_num_turns();
            ctx->encoder_ctx.theta_m_start = ctx->encoder_ctx.theta_m_unwrap;
            ctx->encoder_ctx.theta_e_acc = 0;
            ctx->encoder_ctx.theta_e_raw = 0;
            if (!ctx->encoder_ctx.forward_done)
                ctx->encoder_ctx.step = 2;
            else if (!ctx->encoder_ctx.backward_done)
                ctx->encoder_ctx.step = 3;
            else
                ctx->encoder_ctx.step = 4;
        }
        break;

    case 2:
    case 3:
    {
        float vel = EC_OPEN_LOOP_OMEGA;
        if (ctx->encoder_ctx.step == 3)
            vel = -vel;
        foc_set_ol_vel_cur(vel, ctx->encoder_ctx.cur_test);
        const float STEP = EC_OPEN_LOOP_OMEGA * T_CON * EC_FREQ_F;
        ctx->encoder_ctx.theta_e_acc += (ctx->encoder_ctx.step == 3) ? -STEP : STEP;
        ctx->encoder_ctx.theta_m_unwrap = theta_m + MATH_2PI * encoder_get_num_turns();
        if (FABSF(ctx->encoder_ctx.theta_e_acc - ctx->encoder_ctx.theta_e_raw) >= 0.1745f)
        {
            ctx->encoder_ctx.theta_e_raw = ctx->encoder_ctx.theta_e_acc;
            u8 i = ctx->encoder_ctx.step - 2;
            ctx->encoder_ctx.sum_m[i] += ctx->encoder_ctx.theta_m_unwrap;
            ctx->encoder_ctx.sum_e[i] += ctx->encoder_ctx.theta_e_acc;
            ctx->encoder_ctx.sum_me[i] += ctx->encoder_ctx.theta_m_unwrap * ctx->encoder_ctx.theta_e_acc;
            ctx->encoder_ctx.sum_mm[i] += ctx->encoder_ctx.theta_m_unwrap * ctx->encoder_ctx.theta_m_unwrap;
            ctx->encoder_ctx.cnt[i]++;
        }
        if (FABSF(ctx->encoder_ctx.theta_m_unwrap - ctx->encoder_ctx.theta_m_start) > 6.300f)
        {
            if (ctx->encoder_ctx.step == 2)
                ctx->encoder_ctx.forward_done = true;
            else
                ctx->encoder_ctx.backward_done = true;
            ctx->encoder_ctx.step = 1;
            ctx->steady_tick = 0;
        }
        break;
    }

    case 4:
        foc_set_ol_theta_cur(0.0f, ctx->encoder_ctx.cur_test);
        if (ctx->steady_tick >= EC_ALIGN_ms)
        {
            ctx->steady_tick = 0;
            ctx->encoder_ctx.theta_m_unwrap = theta_m + MATH_2PI * encoder_get_num_turns();
            float diff = ctx->encoder_ctx.theta_m_unwrap - ctx->encoder_ctx.theta_m_start;
            if (diff > 0.0873f)
                temp_params.theta_elec_need_180 = true;
            else if (diff < -0.0873f)
                temp_params.theta_elec_need_180 = false;
            else
            {
                ctx->fault = FAULT_MOTOR_LOCK;
                ctx->state = TUNE_FAILED;
                return true;
            }
            ctx->encoder_ctx.step = 5;
        }
        break;

    case 5:
        _fit_from_sums(ctx->encoder_ctx.sum_m[0], ctx->encoder_ctx.sum_e[0],
                       ctx->encoder_ctx.sum_me[0], ctx->encoder_ctx.sum_mm[0],
                       ctx->encoder_ctx.cnt[0], &ctx->encoder_ctx.k[0], &ctx->encoder_ctx.b[0], &ctx->encoder_ctx.err[0]);
        _fit_from_sums(ctx->encoder_ctx.sum_m[1], ctx->encoder_ctx.sum_e[1],
                       ctx->encoder_ctx.sum_me[1], ctx->encoder_ctx.sum_mm[1],
                       ctx->encoder_ctx.cnt[1], &ctx->encoder_ctx.k[1], &ctx->encoder_ctx.b[1], &ctx->encoder_ctx.err[1]);
        if (ctx->encoder_ctx.err[0] > EC_FIT_MAX_ERROR || ctx->encoder_ctx.err[1] > EC_FIT_MAX_ERROR)
        {
            ctx->fault = FAULT_ENCODER_CAL_FAIL;
            ctx->state = TUNE_FAILED;
            ctx->encoder_ctx.step = 0;
            return true;
        }
        float p_est = (fabsf(ctx->encoder_ctx.k[0]) + fabsf(ctx->encoder_ctx.k[1])) * 0.5f;
        temp_params.pole_pairs = (u8)(p_est + 0.5f);
        if (temp_params.pole_pairs < EC_MIN_POLE_PAIRS || temp_params.pole_pairs > EC_MAX_POLE_PAIRS)
        {
            ctx->fault = FAULT_ENCODER_CAL_FAIL;
            ctx->state = TUNE_FAILED;
            return true;
        }
        if (temp_params.pole_pairs != g_Param.motor_polepairs)
        {
            ctx->fault = FAULT_POLE_PAIR_MISMATCH;
            ctx->state = TUNE_FAILED;
            return true;
        }
        temp_params.direction = (ctx->encoder_ctx.k[0] > 0);
        float o_est_f = -(ctx->encoder_ctx.b[0] / ctx->encoder_ctx.k[0]);
        float o_est_b = -(ctx->encoder_ctx.b[1] / ctx->encoder_ctx.k[1]);
        temp_params.theta_offset = normalize_angle_360((o_est_f + o_est_b) * 0.5f);
        ctx->encoder_ctx.step = 6;
        break;

    case 6:
        foc_set_target((float[2]){0, 0});
        return true;
    }
    return false;
}
static bool _tune_psi_f()
{
    // TODO: SMO 高速整定框架
    // 1. 高速运行 (例如 1000rpm+)，SMO 估算反电动势
    // 2. ke = |E| / omega_elec
    // 3. psi_f = Ke / pole_pairs

    // 临时占位：使用 KV 反推（后续替换为 SMO 实测）
    temp_params.ke = 60.0f / (2.0f * MATH_PI * temp_params.kv * temp_params.pole_pairs);
    temp_params.psi_f = temp_params.ke / temp_params.pole_pairs;
    return true;
}

// ================================= 转动惯量/摩擦系数整定 (框架) =================================
static bool _tune_jb()
{
    // TODO: 阶跃响应法框架
    // 1. 施加阶跃转矩 (例如 iq=2A)
    // 2. 记录加速度曲线: alpha = d(omega)/dt
    // 3. j = T / alpha (忽略摩擦), b = (T - J*alpha) / omega (匀速段)

    // 临时占位：使用默认值跳过
    temp_params.j = 0.0001f;
    temp_params.b = 0.001f;
    return true;
}

// ================================= 整定主循环 (状态切换时初始化) =================================
eTuneState tune_main_loop(tFOC_val *foc_val)
{
    tTuneContext *ctx = &tune_ctx;
    ctx->steady_tick++; // 作为全局稳态计时器
    switch (ctx->state)
    {
    case TUNE_INIT:
        motor_param_tune_init();
        // TODO: 后续添加无感整定 可以添加HFI 和 SMO 参数整定
        foc_set_sensor_mode(ENCODER_CONTROL);
        foc_set_run_mode(OPEN_VOL); // 先切开环电压
        ctx->steady_tick = 0;
        ctx->state = TUNE_IDLE;
        break;
    case TUNE_IDLE:
        if (ctx->steady_tick < TUNE_WAIT_TICKS)
            break; // 先静止等待参数稳定

        // 检查校准电流锚点：tune_cur_limit <= 0 时无法校准 指向电机堵转
        if (temp_params.tune_cur_limit <= 0.0f)
        {
            ctx->fault = FAULT_MOTOR_LOCK;
            ctx->state = TUNE_FAILED;
            break;
        }

        if (false == bsp_adc_calibrate_current(&temp_params.uadc_offset, &temp_params.vadc_offset, &temp_params.wadc_offset))
            break;      // 电流校准
        filter_reset(); // 对电流滤波器进行复位

        // ========== 预计算：Rs 校准阶段所需阈值 (一次计算，整个阶段不变) ==========
        {
            float cur_lim = temp_params.tune_cur_limit;
            float bus_v = foc_val->udc;

            ctx->rs_ctx.i_target = cur_lim * RS_I_TARGET_1_COEF;
            ctx->rs_ctx.i_target_2 = cur_lim * RS_I_TARGET_2_COEF;
            ctx->rs_ctx.hyst_band = cur_lim * RS_HYST_BAND_COEF;
            ctx->rs_ctx.v_limit = bus_v * RS_V_LIMIT_COEF;
            ctx->rs_ctx.steady_err = cur_lim * RS_STEADY_ERR_THR_COEF;
            ctx->rs_ctx.min_delta_i = cur_lim * RS_MIN_DELTA_I_COEF;
            ctx->rs_ctx.v_cmd = 0;
            ctx->rs_ctx.step = 0;

            // 对齐时间：按电流比例缩放
            float align_scale = cur_lim / ALIGN_CUR_REF;
            if (align_scale < 1.0f)
                align_scale = 1.0f;
            ctx->align_total_ticks = (u32)(MS_TO_TICK(ALIGN_TIME_MS_BASE) * align_scale);
        }
        ctx->steady_tick = 0;
        ctx->timeout_tick = 0;

        ctx->state = TUNE_RESISTANCE;
        break;

    case TUNE_RESISTANCE:
        if (ctx->tune_round == 0)
        {
            // 第一轮：开环电压法测 Rs
            if (_tune_rs_ol_vol(foc_val->ialpha))
            {
                if (ctx->fault != FAULT_NONE)
                {
                    ctx->state = TUNE_FAILED;
                    break;
                }

                // 预计算电感阈值
                {
                    float cur_lim = temp_params.tune_cur_limit;
                    float rs_v_ref = temp_params.rs * cur_lim;
                    float bus_v = foc_val->udc;
                    ctx->ls_ctx.v_inj = rs_v_ref * LS_V_START_COEF;
                    if (ctx->ls_ctx.v_inj < LS_V_START_MIN)
                        ctx->ls_ctx.v_inj = LS_V_START_MIN;
                    float ls_v_max = rs_v_ref * LS_V_MAX_COEF;
                    float ls_v_bus = bus_v * LS_V_LIMIT_BUS_COEF;
                    if (ls_v_max > ls_v_bus)
                        ls_v_max = ls_v_bus;
                    ctx->ls_ctx.v_max = ls_v_max;
                    ctx->ls_ctx.i_target = cur_lim * LS_I_TARGET_COEF;
                    ctx->ls_ctx.i_hyst_lo = ctx->ls_ctx.i_target * (1.0f - LS_I_TARGET_HYST);
                    ctx->ls_ctx.i_hyst_hi = ctx->ls_ctx.i_target * (1.0f + LS_I_TARGET_HYST);
                    ctx->ls_ctx.v_step = LS_V_ADJ_STEP;
                    ctx->ls_ctx.ready = false;
                }
                ctx->steady_tick = 0;
                ctx->timeout_tick = 0;
                ctx->state = TUNE_INDUCTANCE;
            }
        }
        else
        {
            // 第二轮：开环电流三相电阻评估
            if (_tune_rs_ol_cur(foc_val))
            {
                if (ctx->fault != FAULT_NONE)
                {
                    ctx->state = TUNE_FAILED;
                    break;
                }

                // 更新参数，切回开环电压重跑电感
                calculate_control_params();
                motor_param_tune_force_save();

                foc_set_run_mode(OPEN_VOL);
                ctx->ls_ctx.state = 0;
                ctx->ls_ctx.ready = false;
                ctx->steady_tick = 0;
                ctx->timeout_tick = 0;
                ctx->state = TUNE_INDUCTANCE;
            }
        }
        break;

    case TUNE_INDUCTANCE:
        if (_TuneLs(foc_val->ualpha, foc_val->ubeta, foc_val->ialpha, foc_val->ibeta))
        {
            ctx->rs_ctx.step_ticks = 0;
            if (ctx->fault != FAULT_NONE)
            {
                ctx->state = TUNE_FAILED;
                break;
            }

            calculate_control_params();
            motor_param_tune_force_save();

            if (ctx->tune_round == 0)
            {
                // 第一轮结束 → 第二轮：切开环电流做三相电阻评估
                ctx->tune_round = 1;
                foc_set_run_mode(OPEN_CUR);
                ctx->rs_ctx.ol_stage = 0;
                ctx->steady_tick = 0;
                ctx->timeout_tick = 0;
                ctx->state = TUNE_RESISTANCE;
            }
            else
            {
                // 第二轮结束 → 编码器校准
                foc_set_run_mode(OPEN_CUR);
                encoder_set_angle_zero();
                ctx->encoder_ctx.theta_elec = 0;
                ctx->encoder_ctx.cur_test = temp_params.tune_cur_limit * 0.1f;
                ctx->encoder_ctx.forward_done = false;
                ctx->encoder_ctx.backward_done = false;
                ctx->encoder_ctx.step = 0;
                ctx->steady_tick = 0;
                ctx->timeout_tick = 0;
                ctx->state = TUNE_ENCODER;
            }
        }
        break;

    case TUNE_ENCODER:
        if (_tune_encoder(foc_val->theta_mech))
        {
            if (ctx->fault != FAULT_NONE)
            {
                ctx->state = TUNE_FAILED;
                break;
            }

            ctx->psi_ctx.sum_e_mag = 0;
            ctx->psi_ctx.sum_vel = 0;
            ctx->psi_ctx.valid_cnt = 0;
            ctx->psi_ctx.ready = false;
            ctx->steady_tick = 0;

            ctx->state = TUNE_ELEC_PARAM;
        }
        break;

    case TUNE_ELEC_PARAM:
        if (_tune_psi_f())
        {
            if (ctx->fault != FAULT_NONE)
            {
                ctx->state = TUNE_FAILED;
                break;
            }

            //  磁链完成：初始化机械参数上下文
            ctx->jb_ctx.accel_phase = false;
            ctx->jb_ctx.sample_cnt = 0;
            ctx->jb_ctx.sum_torque = 0;
            ctx->jb_ctx.sum_accel = 0;
            ctx->jb_ctx.ready = false;
            ctx->steady_tick = 0;

            ctx->state = TUNE_MECH_PARAM;
        }
        break;

    case TUNE_MECH_PARAM:
        if (_tune_jb())
        {
            if (ctx->fault != FAULT_NONE)
            {
                ctx->state = TUNE_FAILED;
                break;
            }

            // todo:这里可以对速度环PI和位置环PID 参数进行调节
            //  结束：保存参数并进入完成状态

            motor_param_tune_force_save();
            ctx->state = TUNE_DONE;
        }
        break;

    default:
        break;
    }
    return ctx->state;
}

// ================================= 辅助接口 =================================
eFaultState tune_get_fault(void) { return tune_ctx.fault; }