#include "tune.h"

#include "bsp_cfg.h"
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

void motor_param_tune_force_save(tParameter *p, tTuneParams *tp)
{
    //  这里是集中写入参数flash中转站
    p->motor_rs = tune->rs_ctx.rs_out;
    p->motor_ld = tune->ls_ctx.ldq_out[0];
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
                       float ts, tCore *core, tFOC *foc)
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
            tTune_rs_ov_cfg rs_ov_cfg = {
                .vada[0].i_hyst_hi = tune->cur_limit * RS_I_TARGET_1_COEF * (1 + RS_I_TARGET_HYST),
                .vada[0].i_hyst_lo = tune->cur_limit * RS_I_TARGET_1_COEF * (1 - RS_I_TARGET_HYST),
                .vada[0].v_hyst_hi = RS_V_ADJ_MAX,
                .vada[0].v_hyst_lo = RS_V_ADJ_START,
                .vada[0].v_step = RS_V_ADJ_STEP,
                .vada[1].i_hyst_hi = tune->cur_limit * RS_I_TARGET_2_COEF * (1 + RS_I_TARGET_HYST),
                .vada[1].i_hyst_lo = tune->cur_limit * RS_I_TARGET_2_COEF * (1 - RS_I_TARGET_HYST),
                .vada[1].v_hyst_hi = RS_V_ADJ_MAX,
                .vada[1].v_hyst_lo = RS_V_ADJ_START,
                .vada[1].v_step = RS_V_ADJ_STEP,

                .cur_steady_err = tune->cur_limit * RS_STEADY_ERR_THR_COEF,
                .steady_ticks = RS_STEADY_MS / 1000.0f / ts,
                .rs_min = RS_RANGE_MIN,
                .rs_max = RS_RANGE_MAX,
                .rs_phase_diff_thr_coef = RS_PHASE_DIFF_THR_COEF,
            };
            tune_rs_ov_init(&tune->rs_ctx, rs_ov_cfg);

            tune->TO_init = true;
        }
        else
        {
            eTuneOneState sta = tune_rs_ov_update(&tune->rs_ctx, core->fb.id, core->fb.ud); // 电阻参数辨识
            if (TO_DONE == sta)
            {
                tune->state = TUNE_INDUCTANCE;
                tune->TO_init = false;
            }
            else if (TO_RUNNING == sta)
            {
                core_ov_cmd_set(tune->rs_ctx.cmd.ud, 0.0f, tune->rs_ctx.cmd.theta_e);
            }
            else
            {
                if (TO_DATA_INVALID == sta)
                    tune->fault = FAULT_RS_TUNE;
                else if (TO_DATA_IMBALANCE == sta)
                    tune->fault = FAULT_RS_IMBALANCE;

                tune->state = TUNE_FAILED;
            }
        }
        break;
    case TUNE_INDUCTANCE:
        if (!tune->TO_init)
        {
            tTune_ls_hfi_cfg ls_hfi_cfg = {
                .vada.i_hyst_hi = tune->cur_limit * (1 + LS_I_TARGET_HYST) * LS_I_TARGET_COEF,
                .vada.i_hyst_lo = tune->cur_limit * (1 - LS_I_TARGET_HYST) * LS_I_TARGET_COEF,
                .vada.v_hyst_hi = LS_V_LIMIT,
                .vada.v_hyst_lo = LS_V_START_MIN,
                .vada.v_step = LS_V_ADJ_STEP,
                .omega_h = MATH_2PI * LS_INJECT_FREQ_HZ,
                .omega_dt = MATH_2PI * LS_INJECT_FREQ_HZ * ts,
                .n_per_cycle = (uint16_t)(1.0f / (LS_INJECT_FREQ_HZ * ts) + 0.5f),
                .align_ticks = LS_ALIGN_MS / 1000.0f / ts,
                .ls_min = LS_RANGE_MIN,
                .ls_max = LS_RANGE_MAX,
                .avg_cycles = LS_AVG_CYCLES,
            };
            tune_ldq_hfi_init(&tune->ls_ctx, ls_hfi_cfg);
            tune->TO_init = true;
        }
        else
        {
            eTuneOneState sta = tune_ldq_hfi(&tune->ls_ctx, core->fb.id, core->fb.iq, core->fb.ud, core->fb.ud); // 电感参数辨识
            if (TO_DONE == sta)
            {
                tune->state = TUNE_ENCODER;
                tune->TO_init = false;

                // 内环参数经验公式  电流环带宽系数暂定为0.5服务后面的校准过程
                float fn_d = 1 / (MATH_2PI * tune->ls_ctx.ldq_out[0] / tune->rs_ctx.rs_out);
                tune->dclbw = MATH_2PI * (2 * fn_d < F_PWM / 10 ? 2 * fn_d : F_PWM / 10);
                float id_kp = 0.5f * tune->dclbw * tune->ls_ctx.ldq_out[0];
                float id_ki = 0.5f * tune->dclbw * tune->rs_ctx.rs_out;

                float fn_q = 1 / (MATH_2PI * tune->ls_ctx.ldq_out[1] / tune->rs_ctx.rs_out);
                tune->qclbw = MATH_2PI * (2 * fn_q < F_PWM / 10 ? 2 * fn_q : F_PWM / 10);
                float iq_kp = 0.5f * tune->qclbw * tune->ls_ctx.ldq_out[1];
                float iq_ki = 0.5f * tune->qclbw * tune->rs_ctx.rs_out;

                // 取电流环开环截止频率 (Hz)
                float f_c_open = fminf(tune->dclbw, tune->qclbw) / MATH_2PI;

                // 设计滤波截止频率：取开环截止频率的 4倍
                float f_filter = 4.0f * f_c_open;

                // 上下限约束
                float f_sw = F_PWM;            // 这里和电流环同频
                float f_max = f_sw / 8.0f;     // 最大允许滤波截止频率（保证滤除开关纹波）
                float f_min = 2.0f * f_c_open; // 最小允许值（避免影响环路）

                if (f_filter > f_max)
                    f_filter = f_max;
                if (f_filter < f_min)
                    f_filter = f_min;

                // 计算一阶低通滤波系数 alpha
                tune->cf_alpha = 1.0f - expf(-MATH_2PI * f_filter / f_sw);
                current_filter_init(tune->cf_alpha);
                // 直接写入
                foc->PI_id.kp = id_kp;
                foc->PI_id.ki = id_ki;
                foc->PI_iq.kp = iq_kp;
                foc->PI_iq.ki = iq_ki;
            }
            else if (TO_RUNNING == sta)
            { // 输出
                core_ov_cmd_set(tune->ls_ctx.cmd.udq[0], tune->ls_ctx.cmd.udq[1], tune->ls_ctx.cmd.theta);
            }
            else
            {
                tune->fault = FAULT_LS_TUNE;
                tune->state = TUNE_FAILED;
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
                .pole_pairs_expected = tune->pole_pairs,
                .fit_max_mse = EC_FIT_MAX_MSE,

            };
            enc_cal_init(&tune->enc_ctx, enc_cal_cfg);
            tune->TO_init = true;
            // 先失能 后面转为闭环电流 开环角度
            core_disable();
        }
        else
        {
            eTuneOneState sta = enc_cal_update(&tune->enc_ctx, core->fb.pos); // 编码器校准
            if (TO_DONE == sta)
            {
                tune->state = TUNE_ELEC_PARAM;
                tune->TO_init = false;
                // TODO: 处理参数
            }
            else if (TO_RUNNING == sta)
            {
                // 电角度开环
                core_ot_cmd_set_theta(tune->enc_ctx.cmd.theta_e);
                tCmd cmd = {
                    .mode = CURRENT_MODE,
                    .id = tune->enc_ctx.cmd.id,
                    .iq = 0.0f,
                };
                core_cmd_set(&cmd);
            }
            else
            {
                if (TO_DATA_INVALID == sta)
                    tune->fault = FAULT_OBS_TUNE;
                else if (TO_DATA_IMBALANCE == sta)
                    tune->fault = FAULT_POLE_PAIR_MISMATCH;
                else if (TO_TIMEOUT == sta)
                    tune->fault = FAULT_MOTOR_LOCK;
                tune->state = TUNE_FAILED;
            }
        }
        break;
    case TUNE_ELEC_PARAM:
        if (!tune->TO_init)
        {
            tTune_psif_cfg psif_cfg = {
                .rs_known = tune->rs_ctx.rs_out,
                .vel_low = PSIF_VEL_LOW,
                .vel_high = PSIF_VEL_HIGH,
                .num_points = PSIF_NUM_POINTS,
                .steady_ticks = PSIF_STEADY_MS / 1000.0f / ts,
                .sample_ticks = PSIF_SAMPLE_MS / 1000.0f / ts,
                .vel_band = PSIF_VEL_BAND,
                .pole_pairs = tune->pole_pairs,
            };
            tune_psif_init(&tune->psif_ctx, psif_cfg);
            tune->TO_init = true;
        }
        else
        {
            eTuneOneState sta = tune_psif_update(&tune->psif_ctx, core->fb.uq, core->fb.iq, core->fb.vel); // 电气参数辨识
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
                .psi_f = tune->psif_ctx.out.psi_f,
                .pole_pairs = tune->enc_ctx.out.pole_pairs,
                .iq_high = tune->cur_limit * JB_IQ_HIGH_COEF,
                .vel_max = JB_VEL_MAX,
                .vel_settle = JB_VEL_SETTLE,
                .alpha_lpf = JB_ALPHA_LPF,
                .skip_ticks = JB_SKIP_MS / 1000.0f / ts,
                .min_samples = JB_MIN_SAMPLES,
            };
            tune_jb_init(&tune->jb_ctx, jb_cfg);
            tune->TO_init = true;
        }
        else
        {
            eTuneOneState sta = tune_jb_update(&tune->jb_ctx, core->fb.vel, core->fb.iq, ts); // 机械参数辨识
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
        core_enter_fault(tune->fault);
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
            float bus_v = core->fb.udc;

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
            if (_tune_rs_ol_vol(core->fb.ialpha))
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
                    float bus_v = core->fb.udc;
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
        if (_TuneLs(core->fb.ualpha, core->fb.ubeta, core->fb.ialpha, core->fb.ibeta))
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
        if (_tune_encoder(core->fb.theta_mech))
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
