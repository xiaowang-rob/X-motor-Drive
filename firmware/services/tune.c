#include "tune.h"
#include "bsp_cfg.h"

// 只适配外部开环命令；core 中 duty -> ud 的执行链仍需单独接入。
static void tune_open_cmd_apply(float duty, float theta_e) {
    tCmdValue cmd = {0};
    cmd.mode = CORE_OPENLOOP;
    cmd.openloop.vel_ctrl = false;
    cmd.openloop.value = theta_e;
    cmd.openloop.duty = duty;
    core_cmd_set(&cmd);
}

void tune_start(tTune *tune, eTuneType type, float cur_limit) {
    tune->state = TUNE_RUNNING;
    tune->type = type;
    tune->TO_init = false;
    tune->cur_limit = cur_limit;
}

void tune_reset(tTune *tune) {
    tune->fault = FAULT_NONE;
    tune->state = TUNE_IDLE;
}

// foc 内参辨识 rs ldq
bool tune_RL_update(tTune *tune, tParameter *p, tCtrl *con, float ts, float id,
                    float iq, float ud, float uq) {
    if (!isfinite(ts) || ts <= 0.0f)
        return false;
    switch (tune->step) {
    case 0: // 一个状态机完成 Rs/Ld/Lq，直接接收上一拍限幅后的 ud/uq。
        if (!tune->TO_init) {
            float samples = 1.0f / (LS_INJECT_FREQ_HZ * ts);
            uint16_t n = (uint16_t)fminf(65535.0f, fmaxf(8.0f, samples + 0.5f));
            tTune_rl_cfg cfg = {
                .adapt =
                    {
                        .i_target_max = tune->cur_limit,
                        .duty_max = RL_DUTY_MAX,
                        .settle_ticks =
                            (uint32_t)(DUTY_SETTLE_MS / 1000.0f / ts),
                    },
                .cur_steady_err = tune->cur_limit * RS_STEADY_ERR_THR_COEF,
                .steady_ticks = (uint32_t)(RS_STEADY_MS / 1000.0f / ts),
                .align_ticks = (uint32_t)(LS_ALIGN_MS / 1000.0f / ts),
                .timeout_ticks = (uint32_t)(10.0f / ts),
                .rs_min = RS_RANGE_MIN,
                .rs_max = RS_RANGE_MAX,
                .rs_phase_diff_thr_coef = RS_PHASE_DIFF_THR_COEF,
                .omega_h = XMATH_2PI / (n * ts),
                .omega_dt = XMATH_2PI / n,
                .n_per_cycle = n,
                .avg_cycles = LS_AVG_CYCLES,
                .ls_min = LS_RANGE_MIN,
                .ls_max = LS_RANGE_MAX,
            };
            tune_rl_init(&tune->rl_ctx, cfg);
            tune->TO_init = true;
            tune_open_cmd_apply(tune->rl_ctx.cmd.duty,
                                tune->rl_ctx.cmd.theta_e);
        } else {
            eTuneOneState sta = tune_rl_update(&tune->rl_ctx, id, iq, ud, uq);
            tune_open_cmd_apply(tune->rl_ctx.cmd.duty,
                                tune->rl_ctx.cmd.theta_e);
            if (sta == TO_DONE) {
                tune->step = 1;
                tune->TO_init = false;
            } else if (sta != TO_RUNNING) {
                tune->fault = sta == TO_DATA_IMBALANCE
                                  ? FAULT_RS_IMBALANCE
                                  : (tune->rl_ctx.step < 3 ? FAULT_RS_TUNE
                                                           : FAULT_LS_TUNE);
                tune->state = TUNE_FAILED;
            }
        }
        break;
    case 1: // 内环pi参数写入
        p->motor_rs = tune->rl_ctx.out.rs;
        p->motor_ld = tune->rl_ctx.out.ldq[0];
        p->motor_lq = tune->rl_ctx.out.ldq[1];

        // 内环参数经验公式  电流环带宽系数重新整定之后默认为0.5
        // 服务后面的校准过程
        p->clbw_coef = 0.5f;
        float fn_d = 1 / (XMATH_2PI * p->motor_ld / p->motor_rs);
        p->dclbw = XMATH_2PI * (2 * fn_d < F_PWM / 10 ? 2 * fn_d : F_PWM / 10);
        p->dclkp = p->clbw_coef * p->dclbw * p->motor_ld;
        p->dclki = p->clbw_coef * p->dclbw * p->motor_rs;

        float fn_q = 1 / (XMATH_2PI * p->motor_lq / p->motor_rs);
        p->qclbw = XMATH_2PI * (2 * fn_q < F_PWM / 10 ? 2 * fn_q : F_PWM / 10);
        p->qclkp = p->clbw_coef * p->qclbw * p->motor_lq;
        p->qclki = p->clbw_coef * p->qclbw * p->motor_rs;

        // 取电流环开环截止频率 (Hz)
        float f_c_open = fminf(p->dclbw, p->qclbw) / XMATH_2PI;

        // 设计滤波截止频率：取开环截止频率的 4倍
        float f_filter = 4.0f * f_c_open;

        // 上下限约束
        float f_sw = F_PWM;        // 这里和电流环同频
        float f_max = f_sw / 8.0f; // 最大允许滤波截止频率（保证滤除开关纹波）
        float f_min = 2.0f * f_c_open; // 最小允许值（避免影响环路）

        if (f_filter > f_max)
            f_filter = f_max;
        if (f_filter < f_min)
            f_filter = f_min;

        // 计算一阶低通滤波系数 alpha
        p->cfalpha = 1.0f - expf(-XMATH_2PI * f_filter / f_sw);
        // 直接写入
        current_filter_init(p->cfalpha);
        con->PI_id.kp = p->dclkp;
        con->PI_id.ki = p->dclki;
        con->PI_iq.kp = p->qclkp;
        con->PI_iq.ki = p->qclki;

        return true;
    }
    return false;
}

bool tune_flux_update(tTune *tune, tParameter *p, float uq, float iq, float vel,
                      float ts) {

    if (!tune->TO_init) {
        tTune_psif_cfg psif_cfg = {
            .rs_known = p->motor_rs,
            .vel_low = PSIF_VEL_LOW,
            .vel_high = PSIF_VEL_HIGH,
            .num_points = PSIF_NUM_POINTS,
            .steady_ticks = PSIF_STEADY_MS / 1000.0f / ts,
            .sample_ticks = PSIF_SAMPLE_MS / 1000.0f / ts,
            .vel_band = PSIF_VEL_BAND,
            .timeout_ticks = (uint32_t)(10.0f / ts),
            .fit_max_mse = 0.01f, // V²；台架上按实际电压噪声校验
            .pole_pairs = p->motor_polepairs,
        };
        tune_psif_init(&tune->psif_ctx, psif_cfg);
        tune->TO_init = true;
    } else {
        eTuneOneState sta = tune_psif_update(&tune->psif_ctx, uq, iq, vel);
        if (TO_DONE == sta) {
            tune->step = 0;
            tune->state = TUNE_DONE;
            tune->TO_init = false;
            p->motor_psif = tune->psif_ctx.out.psi_f;
            p->motor_ke = tune->psif_ctx.out.ke;
            float ke_ext = 9.55f / p->motor_kv;
            if (XFABSF(ke_ext - p->motor_ke) > 0.1f) {
                tune->fault = FAULT_PSIF_TUNE;
                tune->state = TUNE_FAILED;
            } else
                tune->state = TUNE_DONE;
            tune->TO_init = false;
            return true;
        } else if (TO_RUNNING == sta) {

        } else {
        }
    }
    return false;
}

// 机械参数辨识
bool tune_mech_update(tTune *tune, tParameter *p, float ts, float iq,
                      float vel) {
    if (!tune->TO_init) {
        tTune_JB_cfg jb_cfg = {
            .psi_f = tune->psif_ctx.out.psi_f,
            .pole_pairs = tune->enc_ctx.out.pole_pairs,
            .iq_high = tune->cur_limit * JB_IQ_HIGH_COEF,
            .vel_max = JB_VEL_MAX,
            .vel_settle = JB_VEL_SETTLE,
            .alpha_lpf = JB_ALPHA_LPF,
            .skip_ticks = JB_SKIP_MS / 1000.0f / ts,
            .min_samples = JB_MIN_SAMPLES,
            .timeout_ticks = (uint32_t)(10.0f / ts),
        };
        tune_jb_init(&tune->jb_ctx, jb_cfg);
        tune->TO_init = true;
    } else {
        eTuneOneState sta =
            tune_jb_update(&tune->jb_ctx, vel, iq, ts); // 机械参数辨识

        if (TO_DONE == sta) {
            tune->state = TUNE_DONE;
            tune->TO_init = false;
            // TODO: 处理参数
        } else {
        }
    }
    return false;
}

// 观测器参数辨识
bool tune_obs_update(tTune *tune, tXFocCore *core, eObsList obs, tParameter *p,
                     float ts) {
    switch (obs) {
    case OBS_NONE:

        break;
    case OBS_ENC_HALL:

        break;
    case OBS_ENC_ABI:

        break;
    case OBS_INT_ENC:
    case OBS_ENC_SPI:
    case OBS_ENC_SINCOS:
        if (!tune->TO_init) {
            tEncCal_cfg enc_cal_cfg = {
                .adapt =
                    {
                        .i_target_max = tune->cur_limit,
                        .duty_max = RL_DUTY_MAX,
                        .settle_ticks =
                            (uint32_t)(DUTY_SETTLE_MS / 1000.0f / ts),
                    },
                .align_ticks = EC_ALIGN_MS / 1000.0f / ts,
                .timeout_ticks = (uint32_t)(60.0f / ts),
                .delta_e = EC_OPEN_LOOP_OMEGA * ts,
                .sample_step_e = EC_SAMPLE_STEP_E,
                .travel_pos = EC_TRAVEL_POS,
                .pole_pairs_expected = p->motor_polepairs,
                .fit_max_mse = EC_FIT_MAX_MSE,

            };
            enc_cal_init(&tune->enc_ctx, enc_cal_cfg);
            tune->TO_init = true;
            tune_open_cmd_apply(tune->enc_ctx.cmd.duty,
                                tune->enc_ctx.cmd.theta_e);
        } else {
            eTuneOneState sta =
                enc_cal_update(&tune->enc_ctx, core->fb.pos, core->fb.id);
            tune_open_cmd_apply(tune->enc_ctx.cmd.duty,
                                tune->enc_ctx.cmd.theta_e);
            if (TO_DONE == sta) {
                tune->state = TUNE_DONE;
                tune->TO_init = false;
                // TODO: 处理参数
            } else {
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
    case OBS_HFI_SMO:
        break;
    }
    return false;
}
// pid参数自适应
bool tune_pid_update() { return false; }
// mit参数自适应
bool tune_mit_update() { return false; }

eTuneState tune_update(tTune *tune, tParameter *p, float ts, tXFocCore *core) {
    switch (tune->state) {
    case TUNE_IDLE:
        break;
    case TUNE_RUNNING:
        switch (tune->type) {
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
