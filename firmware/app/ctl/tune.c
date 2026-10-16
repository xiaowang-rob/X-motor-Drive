#include "tune.h"

#include "bsp_cfg.h"
#include "bsp_math.h"

void tune_start(tTune *tune, eTuneType type, float cur_limit)
{
    tune->state = TUNE_RUNNING;
    tune->type = type;
    tune->TO_init = false;
    tune->cur_limit = cur_limit;
}

void tune_reset(tTune *tune)
{
    tune->fault = FAULT_NONE;
    tune->state = TUNE_IDLE;
}

// foc 内参辨识 rs ldq psif
bool tune_FOC_update(tTune *tune, tCore *core, tCtrl *con, tParameter *p, float ts)
{
    switch (tune->step)
    {
    case 0: // 电阻参数辨识
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
                tune->step = 1;
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
    case 1: // 电感参数辨识
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
                tune->step = 2;
                tune->TO_init = false;
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
    case 2: // 内环pi参数写入

        p->motor_rs = tune->rs_ctx.rs_out;
        p->motor_ld = tune->ls_ctx.ldq_out[0];
        p->motor_lq = tune->ls_ctx.ldq_out[1];

        // 内环参数经验公式  电流环带宽系数重新整定之后默认为0.5 服务后面的校准过程
        p->clbw_coef = 0.5f;
        float fn_d = 1 / (MATH_2PI * p->motor_ld / p->motor_rs);
        p->dclbw = MATH_2PI * (2 * fn_d < F_PWM / 10 ? 2 * fn_d : F_PWM / 10);
        p->dclkp = p->clbw_coef * p->dclbw * p->motor_ld;
        p->dclki = p->clbw_coef * p->dclbw * p->motor_rs;

        float fn_q = 1 / (MATH_2PI * p->motor_lq / p->motor_rs);
        p->qclbw = MATH_2PI * (2 * fn_q < F_PWM / 10 ? 2 * fn_q : F_PWM / 10);
        p->qclkp = p->clbw_coef * p->qclbw * p->motor_lq;
        p->qclki = p->clbw_coef * p->qclbw * p->motor_rs;

        // 取电流环开环截止频率 (Hz)
        float f_c_open = fminf(p->dclbw, p->qclbw) / MATH_2PI;

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
        p->cfalpha = 1.0f - expf(-MATH_2PI * f_filter / f_sw);
        // 直接写入
        current_filter_init(p->cfalpha);
        con->PI_id.kp = p->dclkp;
        con->PI_id.ki = p->dclki;
        con->PI_iq.kp = p->qclkp;
        con->PI_iq.ki = p->qclki;

        tune->step = 3;
        break;
    case 3: // psif 参数辨识

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
                .pole_pairs = p->motor_polepairs,
            };
            tune_psif_init(&tune->psif_ctx, psif_cfg);
            tune->TO_init = true;
        }
        else
        {
            eTuneOneState sta = tune_psif_update(&tune->psif_ctx, core->fb.uq, core->fb.iq, core->fb.vel); // 电气参数辨识
            if (TO_DONE == sta)
            {
                tune->step = 0;
                tune->state = TUNE_DONE;
                tune->TO_init = false;
                p->motor_psif = tune->psif_ctx.out.psi_f;
                p->motor_ke = tune->psif_ctx.out.ke;
                float ke_ext = 9.55f / p->motor_kv;
                if (FABS(ke_ext - p->motor_ke) > 0.1f)
                {
                    tune->fault = FAULT_PSIF_TUNE;
                    tune->state = TUNE_FAILED;
                }
                else
                    tune->state = TUNE_DONE;
                tune->TO_init = false;
                return true;
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
    }
    return false;
}
// 观测器参数辨识
bool tune_obs_update(tTune *tune, tCore *core, tObs *obs, tParameter *p, float ts)
{
    switch (obs->type)
    {
    case NONE_OBS:

        break;
    case ENC_HALL:

        break;
    case ENC_SPI:
    case ENC_SINCOS:
        if (!tune->TO_init)
        {
            tEncCal_cfg enc_cal_cfg = {
                .i_tune = tune->cur_limit,
                .align_ticks = EC_ALIGN_MS / 1000.0f / ts,
                .delta_e = EC_OPEN_LOOP_OMEGA * ts,
                .sample_step_e = EC_SAMPLE_STEP_E,
                .travel_pos = EC_TRAVEL_POS,
                .pole_pairs_expected = p->motor_polepairs,
                .fit_max_mse = EC_FIT_MAX_MSE,

            };
            enc_cal_init(&tune->enc_ctx, enc_cal_cfg);
            tune->TO_init = true;
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
    case HFI_SMO:
        break;
    }
}
// 机械参数辨识
bool tune_mech_update()
{
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
}
// pid参数自适应
bool tune_pid_update()
{
}
// mit参数自适应
bool tune_mit_update()
{
}

eTuneState tune_update(tTune *tune, tParameter *p,
                       float ts, tCore *core)
{

    switch (tune->state)
    {
    case TUNE_IDLE:
        break;
    case TUNE_RUNNING:
        switch (tune->type)
        {
        case TUNE_FOC:

            break;
        case TUNE_OBS:
            break;
        case TUNE_MECH:
            break;
        case TUNE_PID:
            break;
        case TUNE_MIT:
            break;
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
