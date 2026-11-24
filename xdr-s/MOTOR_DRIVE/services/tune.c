#include "tune.h"

#include "bsp_config.h"
#include "protection.h"

static void tune_open_cmd_cfg(tMotorCtrl *motor, bool ol_vel_ctrl) {
    tCmdCfg cfg;
    cfg.mode = MOTOR_OPENLOOP;
    cfg.ol_vel_ctrl = ol_vel_ctrl;
    motor_cmd_cfg(motor, &cfg);
}
static void tune_open_cmd_apply(tMotorCtrl *motor, float duty, float theta_e) {
    tCmdValue cmd = {0};
    cmd.mode = MOTOR_OPENLOOP;
    cmd.openloop.value = theta_e;
    cmd.openloop.duty = duty;
    motor_cmd_set(motor, &cmd);
}

static void tune_current_cmd_cfg(tMotorCtrl *motor) {
    tCmdCfg cfg;
    cfg.mode = MOTOR_CURRENT_MODE;
    cfg.tau_ctrl = false;
    motor_cmd_cfg(motor, &cfg);
}

static void tune_current_cmd_apply(tMotorCtrl *motor, float cur) {
    tCmdValue cmd = {0};
    cmd.mode = MOTOR_CURRENT_MODE;
    cmd.current_ctrl.value = cur;
    motor_cmd_set(motor, &cmd);
}

static void tune_pid_vel_cmd_cfg(tMotorCtrl *motor) {
    tCmdCfg cfg;
    cfg.mode = MOTOR_PID_SPEED;
    cfg.fw_ctrl = false;
    motor_cmd_cfg(motor, &cfg);
}

static void tune_pid_vel_cmd_apply(tMotorCtrl *motor, float vel) {
    tCmdValue cmd = {0};
    cmd.mode = MOTOR_PID_SPEED;
    cmd.vel_pid_ctrl.vel = vel;
    motor_cmd_set(motor, &cmd);
}

void tune_start(tTune *tune, eTuneType type, float cur_limit, float vel_limit) {
    tune->state = TUNE_RUNNING;
    tune->type = type;
    tune->TO_init = false;
    tune->cur_limit = cur_limit;
    tune->vel_limit = vel_limit;
}

void tune_reset(tTune *tune) {
    tune->fault = FAULT_NONE;
    tune->state = TUNE_IDLE;
}

// foc 内参辨识 rs ldq
void tune_RL_update(tTune *tune, tMotorCtrl *motor, tParameter *p, float ts, float Vbus, float id, float iq,
                    float dutyd, float dutyq) {

    switch (tune->step) {
    case 0:
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
            tune_open_cmd_cfg(motor, false);
            tune->TO_init = true;
        } else {
            float ud = Vbus * dutyd;
            float uq = Vbus * dutyq;
            eTuneOneState sta = tune_rl_update(&tune->rl_ctx, id, iq, ud, uq);
            tune_open_cmd_apply(motor, tune->rl_ctx.cmd.duty,
                                tune->rl_ctx.cmd.theta_e);
            if (sta == TO_DONE) {
                tune->step = 1;
                tune->TO_init = false;
            } else if (sta != TO_RUNNING) {
                if (sta == TO_DATA_IMBALANCE)
                    tune->fault = FAULT_RS_IMBALANCE;
                else
                    tune->fault = FAULT_RL_TUNE;
            }
        }
        break;
    case 1: // 内环pi参数写入
        // TODO:电机控制改用占空比输出，这里kpki参数需要 /Vs 最大输出电压
        // Vbus/g3
        p->motor_rs = tune->rl_ctx.out.rs;
        p->motor_ld = tune->rl_ctx.out.ldq[0];
        p->motor_lq = tune->rl_ctx.out.ldq[1];

        // 内环参数经验公式  电流环带宽系数重新整定之后默认为0.5
        // 服务后面的校准过程
        p->clbw_coef = 0.5f;
        float fn_d = 1 / (XMATH_2PI * p->motor_ld / p->motor_rs);
        p->dclbw = XMATH_2PI * (2 * fn_d < F_PWM / 10 ? 2 * fn_d : F_PWM / 10);
        p->dclkp = p->clbw_coef * p->dclbw * p->motor_ld * XMATH_SQRT3 / Vbus;
        p->dclki = p->clbw_coef * p->dclbw * p->motor_rs * XMATH_SQRT3 / Vbus;

        float fn_q = 1 / (XMATH_2PI * p->motor_lq / p->motor_rs);
        p->qclbw = XMATH_2PI * (2 * fn_q < F_PWM / 10 ? 2 * fn_q : F_PWM / 10);
        p->qclkp = p->clbw_coef * p->qclbw * p->motor_lq * XMATH_SQRT3 / Vbus;
        p->qclki = p->clbw_coef * p->qclbw * p->motor_rs * XMATH_SQRT3 / Vbus;

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

        tune->state = TUNE_DONE;
        return;
    }
    return;
}

void tune_flux_update(tTune *tune, tMotorCtrl *motor, tParameter *p, float ts, float Vbus, float dutyq,
                      float iq, float vel) {

    if (!tune->TO_init) {
        tTune_psif_cfg psif_cfg = {
            .rs_known = p->motor_rs,
            .vel_low = tune->vel_limit * PSIF_VEL_LOW_COEF,
            .vel_high = tune->vel_limit,
            .num_points = PSIF_NUM_POINTS,
            .steady_ticks = PSIF_STEADY_MS / 1000.0f / ts,
            .sample_ticks = PSIF_SAMPLE_MS / 1000.0f / ts,
            .vel_band = tune->vel_limit * PSIF_VEL_BAND_COEF,
            .timeout_ticks = (uint32_t)(10.0f / ts),
            .fit_max_mse = 0.01f, // V²；台架上按实际电压噪声校验
            .pole_pairs = p->motor_polepairs,
        };
        tune_psif_init(&tune->psif_ctx, psif_cfg);
        tune_pid_vel_cmd_cfg(motor);
        tune->TO_init = true;
    } else {
        float uq = Vbus * dutyq;
        eTuneOneState sta = tune_psif_update(&tune->psif_ctx, uq, iq, vel);
        tune_pid_vel_cmd_apply(motor, tune->psif_ctx.cmd.vel);
        if (TO_DONE == sta) {
            tune->step = 0;
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
            return;
        } else {
        }
    }
    return;
}

// 机械参数辨识
bool tune_mech_update(tTune *tune, tMotorCtrl *motor, tParameter *p, float ts, float iq, float vel) {
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
bool tune_obs_update(tTune *tune, tMotorCtrl *motor, eEncList enc, tParameter *p, float ts) {
    switch (enc) {
    case ENC_NONE:

        break;
    case ENC_HALL:

        break;
    case ENC_ABI:

        break;
    case ENC_INT:
    case ENC_SPI:
    case ENC_SINCOS:
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
                .delta_e = tune->vel_limit * ts,
                .sample_step_e = EC_SAMPLE_STEP_E,
                .travel_pos = EC_TRAVEL_POS,
                .pole_pairs_expected = p->motor_polepairs,
                .fit_max_mse = EC_FIT_MAX_MSE,

            };
            enc_cal_init(&tune->enc_ctx, enc_cal_cfg);
            tune_open_cmd_cfg(motor, false);
            tune->TO_init = true;

        } else {
            eTuneOneState sta =
                enc_cal_update(&tune->enc_ctx, motor->fb.pos, motor->fb.id);
            tune_open_cmd_apply(motor, tune->enc_ctx.cmd.duty,
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
    }
    return false;
}
// pid参数自适应
bool tune_pid_update() { return false; }
// mit参数自适应
bool tune_mit_update() { return false; }

eTuneState tune_mian_tim_task(tTune *tune, tProtectionManager *pm,
                              tParameter *p, tMotorCtrl *motor, float Vbus,
                              float ts) {
    switch (tune->state) {
    case TUNE_IDLE:
        break;
    case TUNE_RUNNING:
        switch (tune->type) {
        case TUNE_RL:
            tune_RL_update(tune, motor, p,
                           ts,
                           Vbus,
                           motor->fb.id,
                           motor->fb.iq,
                           motor->ref.dutyd,
                           motor->ref.dutyq);
            break;
        case TUNE_Psif:
            tune_flux_update(
                tune, motor, p, ts, Vbus, motor->ref.dutyq, motor->fb.iq, motor->fb.vel);
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
        protection_fault(pm, motor, tune->fault);
        break;
    }
    return tune->state;
}
