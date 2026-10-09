#include "tune.h"

#include "bsp_math.h"

void tune_init(tTune *tune, float cur_limit)
{
    // TODO:后续添加无感整定，根据选择的感应模式校准，无感只校准电机，有感校准电机和编码器，直接校准电机，直接用HFI、SMO做精准的控制

    tune->state = TUNE_INIT;
    tune->TO_init = false;
    tune->cur_limit = cur_limit;

    // 基本参数加载
}

void tune_reset(tTune *tune)
{
    tune->fault = FAULT_NONE;
    tune->state = TUNE_INIT;
}

// 内环参数经验公式 clbw_coef - 电流环带宽系数  fc - 电流环频率
static void calculate_foc_params(tTuneParams *tp, float clbw_coef, float fc)
{
    float fn_d = 1 / (MATH_2PI * tp->ld / tp->rs);
    float wc_d = MATH_2PI * clbw_coef * (2 * fn_d < fc / 10 ? 2 * fn_d : fc / 10);
    tp->id_kp = wc_d * tp->ld;
    tp->id_ki = wc_d * tp->rs;

    float fn_q = 1 / (MATH_2PI * tp->lq / tp->rs);
    float wc_q = MATH_2PI * clbw_coef * (2 * fn_q < fc / 10 ? 2 * fn_q : fc / 10);
    tp->iq_kp = wc_q * tp->lq;
    tp->iq_ki = wc_q * tp->rs;

    // 取电流环开环截止频率 (Hz)
    float f_c_open = fminf(wc_d, wc_q) / MATH_2PI;

    // 设计滤波截止频率：取开环截止频率的 4倍
    float f_filter = 4.0f * f_c_open;

    // 上下限约束
    float f_sw = fc;               // 这里和电流环同频
    float f_max = f_sw / 8.0f;     // 最大允许滤波截止频率（保证滤除开关纹波）
    float f_min = 2.0f * f_c_open; // 最小允许值（避免影响环路）

    if (f_filter > f_max)
        f_filter = f_max;
    if (f_filter < f_min)
        f_filter = f_min;

    // 计算一阶低通滤波系数 alpha
    tp->cur_filter_alpha = 1.0f - expf(-MATH_2PI * f_filter / f_sw);

    // 直接写入
    foc_set_cur_loop_param(tp->id_kp, tp->id_ki, tp->iq_kp, tp->iq_ki);
}

void motor_param_tune_force_save(tParameter *p, tTuneParams *tp)
{
    //  这里是集中写入参数flash中转站
    p->motor_rs = tp->rs;
    p->motor_ld = tp->ld;
    p->motor_lq = tp->lq;
    p->motor_psif = tp->psi_f;
    p->motor_ke = tp->ke;
    p->motor_j = tp->j;
    p->motor_b = tp->b;
    p->motor_polepairs = tp->pole_pairs;
    p->theta_offset = tp->theta_offset;
    p->positive_dir = tp->direction;

    p->qclkp = tp->iq_kp;
    p->qclki = tp->iq_ki;
    p->dclkp = tp->id_kp;
    p->dclki = tp->id_ki;

    p->cfalpha = tp->cur_filter_alpha;
}

eTuneState tune_update(tTune *tune, tParameter *p,
                       float ts, tFOCval *foc_val, tCoreVal *core_val)
{
    switch (tune->state)
    {
    case TUNE_INIT:
        tune_init(tune, p->tune_current);
        // TODO: 后续添加无感整定 可以添加HFI 和 SMO 参数整定

        tune->state = TUNE_RESISTANCE;
        break;
    case TUNE_RESISTANCE:
        if (!tune->TO_init)
        {
            // 电阻参数辨识
            tTune_rs_oc_cfg rs_oc_cfg = {
                .cur_1 = tune->cur_limit * RS_I_TARGET_1_COEF,
                .cur_2 = tune->cur_limit * RS_I_TARGET_2_COEF,
                .cur_steady_err = tune->cur_limit * RS_STEADY_ERR_THR_COEF,
                .steady_ticks = RS_STEADY_MS / 1000.0f / ts,
                .rs_min = RS_RANGE_MIN,
                .rs_max = RS_RANGE_MAX,
                .rs_phase_diff_thr_coef = RS_PHASE_DIFF_THR_COEF,
            };
            tune_rs_oc_init(&tune->rs_ctx, rs_oc_cfg);

            // TODO:切换为开环电压模式
            tune->TO_init = true;
        }
        else
        {
            eTuneOneState sta = tune_rs_oc_update(&tune->rs_ctx, foc_val->id, foc_val->ud); // 电阻参数辨识
            if (TO_DONE == sta)
            {
                tune->state = TUNE_INDUCTANCE;
                tune->TO_init = false;
                // TODO: 处理参数
            }
            else if (TO_RUNNING == sta)
            {
                break;
            }
            else
            {
            }
        }
        break;
    case TUNE_INDUCTANCE:
        if (!tune->TO_init)
        {
            tTune_ls_hfi_cfg ls_hfi_cfg = {
                .omega_h = MATH_2PI * LS_INJECT_FREQ_HZ,
                .omega_dt = MATH_2PI * LS_INJECT_FREQ_HZ * ts,
                .n_per_cycle = (uint16_t)(1.0f / (LS_INJECT_FREQ_HZ * ts) + 0.5f),
                .align_ticks = LS_ALIGN_MS / 1000.0f / ts,
                .v_inj_start = LS_V_START_MIN,
                .v_inj_max = LS_V_LIMIT,
                .v_inj_step = LS_V_ADJ_STEP,
                .i_hyst_lo = tune->cur_limit * (1 - LS_I_TARGET_HYST) * LS_I_TARGET_COEF,
                .i_hyst_hi = tune->cur_limit * (1 + LS_I_TARGET_HYST) * LS_I_TARGET_COEF,

                .ls_min = LS_RANGE_MIN,
                .ls_max = LS_RANGE_MAX,
                .avg_cycles = LS_AVG_CYCLES,
            };
            tune_ldq_hfi_init(&tune->ls_ctx, ls_hfi_cfg);
            tune->TO_init = true;
        }
        else
        {
            eTuneOneState sta = tune_ldq_hfi(&tune->ls_ctx, foc_val->id, foc_val->iq, foc_val->ud, foc_val->ud); // 电感参数辨识
            if (TO_DONE == sta)
            {
                tune->state = TUNE_ENCODER;
                tune->TO_init = false;
                // TODO: 处理参数
            }
            else if (TO_RUNNING == sta)
            {
                break;
            }
            else
            {
            }
        }
        break;
    case TUNE_ENCODER:
        if (!tune->TO_init)
        {
            tEncCal_cfg enc_cal_cfg = {
                .i_tune = tune->cur_limit,
                .align_ticks = EC_ALIGN_MS / 1000.0f / ts,
                .delta_e = EC_OPEN_LOOP_OMEGA * ts,
                .sample_step_e = EC_SAMPLE_STEP_E,
                .travel_pos = EC_TRAVEL_POS,
                .pole_pairs_expected = tune->params.pole_pairs,
                .fit_max_mse = EC_FIT_MAX_MSE,

            };
            enc_cal_init(&tune->enc_ctx, enc_cal_cfg);
            tune->TO_init = true;
        }
        else
        {
            eTuneOneState sta = enc_cal_update(&tune->enc_ctx, core_val->pos); // 编码器校准
            if (TO_DONE == sta)
            {
                tune->state = TUNE_ELEC_PARAM;
                tune->TO_init = false;
                // TODO: 处理参数
            }
            else if (TO_RUNNING == sta)
            {
                break;
            }
            else
            {
            }
        }
        break;
    case TUNE_ELEC_PARAM:
        if (!tune->TO_init)
        {
            tTune_psif_cfg psif_cfg = {
                .rs_known = tune->params.rs,
                .vel_low = PSIF_VEL_LOW,
                .vel_high = PSIF_VEL_HIGH,
                .num_points = PSIF_NUM_POINTS,
                .steady_ticks = PSIF_STEADY_MS / 1000.0f / ts,
                .sample_ticks = PSIF_SAMPLE_MS / 1000.0f / ts,
                .vel_band = PSIF_VEL_BAND,
                .pole_pairs = tune->params.pole_pairs,
            };
            tune_psif_init(&tune->psif_ctx, psif_cfg);
            tune->TO_init = true;
        }
        else
        {
            eTuneOneState sta = psif_update(&tune->psif_ctx, foc_val->uq, foc_val->iq, core_val->vel); // 电气参数辨识
            if (TO_DONE == sta)
            {
                tune->state = TUNE_MECH_PARAM;
                tune->TO_init = false;
                // TODO: 处理参数
            }
            else if (TO_RUNNING == sta)
            {
                break;
            }
            else
            {
            }
        }
        break;

    case TUNE_MECH_PARAM:
        if (!tune->TO_init)
        {
            tTune_JB_cfg jb_cfg = {
                .psi_f = tune->params.psi_f,
                .pole_pairs = tune->params.pole_pairs,
                .iq_high = tune->cur_limit * JB_IQ_HIGH_COEF,
                .vel_max = JB_VEL_MAX,
                .vel_settle = JB_VEL_SETTLE,
                .alpha_lpf = JB_ALPHA_LPF,
                .skip_ticks = JB_SKIP_MS / 1000.0f / ts,
                .min_samples = JB_MIN_SAMPLES,
            };
            jb_init(&tune->jb_ctx, jb_cfg);
            tune->TO_init = true;
        }
        else
        {
            eTuneOneState sta = jb_update(&tune->jb_ctx, core_val->vel, foc_val->iq, ts); // 机械参数辨识
            if (TO_DONE == sta)
            {
                tune->state = TUNE_DONE;
                tune->TO_init = false;
                // TODO: 处理参数
            }
            else if (TO_RUNNING == sta)
            {
                break;
            }
            else
            {
            }
        }
        break;

    case TUNE_DONE:

        break;

    case TUNE_FAILED:

        break;
    }
    return tune->state;
}

// 1、电气参数层辨识 （Ld Lq（高频注入） Rs/Psif （RLS/MRAS））
// 2、编码器校准 （偏移值 极对数 方向）
// 3、机械参数辨识（J TL EKO+MRAS）

// ================================= 初始化与重置 =================================

// ================================= 整定主循环 (状态切换时初始化) =================================
eTuneState tune_main_loop(tFOC_val *foc_val, float ts)
{
    tTuneContext *ctx = &tune_ctx;
    ctx->steady_tick++; // 作为全局稳态计时器
    switch (ctx->state)
    {
    case TUNE_INIT:

    case TUNE_IDLE:
        if (ctx->steady_tick < TUNE_WAIT_TICKS)
            break; // 先静止等待参数稳定

        // 检查校准电流锚点：tune_cur_limit <= 0 时无法校准 指向电机堵转
        if (temp_params.cur_limit <= 0.0f)
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
            float cur_lim = temp_params.cur_limit;
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
                    float cur_lim = temp_params.cur_limit;
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
                ctx->encoder_ctx.cur_test = temp_params.cur_limit * 0.1f;
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
