#include "xfoc_main.h"

#include "xmath.h"

#include "encoder.h"
#include "pwm.h"
#include "sample.h"

tXFocCore core;

void current_filter_init(float cfalpha) {
    filter_first_order_lag_init(&core.cf_u, cfalpha, 0);
    filter_first_order_lag_init(&core.cf_v, cfalpha, 0);
    filter_first_order_lag_init(&core.cf_w, cfalpha, 0);
}

void core_init(tParameter *param, float t_hl, float t_ml, float t_ll) {

    memset(&core.fb, 0, sizeof(core.fb));

    core.enable = false;
    core.state = CORE_INIT;
    core.param = param;
    core.fb.udc = sample_get_vbus(&g_sample);
    core.fb.vmax = core.fb.udc * XMATH_INSQRT3;

    // 使能栅极驱动
    pwm_power_on(&g_pwm, true);
    pwm_enable(&g_pwm, true);
    // 初始化svpwm
    svpwm_init(&core.svpwm, &g_pwm, core.fb.udc);

    // foc初始化
    ctrl_foc_init(&core.ctrl, core.param, t_hl, core.fb.vmax);
    // pid控制器初始化
    ctrl_pid_init(&core.ctrl, core.param, t_ml, t_ll);
    // mit控制器初始化
    ctrl_mit_init(&core.ctrl, core.param);

    // 初始化观测器
    if (!obs_init(&core.obs, core.param)) {
        core_enter_fault(FAULT_OBS_INIT);
        return;
    }

    // 初始化 内置轨迹规划
    traj_init(&core.traj, core.param);

    // 电流滤波初始化
    current_filter_init(param->cfalpha);

    core.state = CORE_IDLE;

    return;
}
void core_reset(void) {

    filter_first_order_lag_reset(&core.cf_u, 0);
    filter_first_order_lag_reset(&core.cf_v, 0);
    filter_first_order_lag_reset(&core.cf_w, 0);

    memset(&core.ref, 0, sizeof(core.ref));
}
// 核心失能
void core_disable(void) {
    core.enable = false;
    core.oc_enable = false;
    core_reset();
    core.state = CORE_IDLE;
    pwm_enable(&g_pwm, false);
}

// 核心使能和指令写入
void core_cmd_set(tCmdValue *cmd) {
    // 非法输入
    if (cmd->mode < CORE_OPENLOOP || cmd->mode > CORE_MIT_MODE) {
        core_enter_warning(WARNING_INVALID_INPUT);
    }
    if (cmd->mode != core.state) {
        if (core.state != CORE_IDLE) {
            core_enter_warning(WARNING_INVALID_INPUT);
        }
        core.state = cmd->mode;
    }

    core.enable = true;
    switch (cmd->mode) {
    case CORE_OPENLOOP:
        if (cmd->openloop.vel_ctrl)
            core.ref.vel_elec = cmd->openloop.value;
        else {
            core.ref.theta_elec = cmd->openloop.value;
            core.ref.vel_elec = 0;
        }
        core.ref.duty = XCLAMP(cmd->openloop.duty, 0, 1);
    case CORE_CURRENT_MODE:
        if (cmd->current_ctrl.tau_ctrl) {
            core.ref.tau = cmd->current_ctrl.value;
            core.ref.iq = core.ref.tau / core.param->motor_ke;
        } else {
            core.ref.iq = cmd->current_ctrl.value;
        }

        break;
    case CORE_PID_SPEED:
        traj_set_target(&core.traj, cmd->vel_pid_ctrl.vel);
        break;
    case CORE_PID_POSITION:
        traj_set_target(&core.traj, cmd->pos_pid_ctrl.pos);
        break;
    case CORE_MIT_MODE:
        if (core.traj.cfg.mode == TRAJ_DISABLE) {
            core.ref.tau_ff = cmd->mit_ctrl.tau_ff;
            core.ref.vel = cmd->mit_ctrl.vel;
            core.ref.pos = cmd->mit_ctrl.pos;
            mit_config(&core.ctrl.mit, cmd->mit_ctrl.kp, cmd->mit_ctrl.kd);
        } else
            traj_set_target(&core.traj, cmd->mit_ctrl.pos);
        break;
    default:
        core_enter_warning(WARNING_INVALID_INPUT);
        break;
    }
}

// 核心进入错误状态
void core_enter_fault(eFault fault) {
    core.enable = false;
    core_reset();
    core.state = CORE_FAULT;
    core.fault = fault;
    pwm_enable(&g_pwm, false);
    pwm_power_on(&g_pwm, false); // 直接电源隔断
}
// 核心进入警告状态
void core_enter_warning(eWarning warning) {
    core.enable = false;
    core_reset();
    core.state = CORE_WARNING;
    core.warning = warning;
    pwm_enable(&g_pwm, false);
}
// 设置零点
void core_set_pos_zero(void) {
    if (core.state != CORE_IDLE) {
        core_enter_warning(WARNING_INVALID_INPUT);
    }
    obs_set_zero(&core.obs);
}
// 设置限位
void core_set_pos_limit(void) {
    if (core.state != CORE_IDLE) {
        core_enter_warning(WARNING_INVALID_INPUT);
    }
    obs_set_pos_limit(&core.obs);
}
// // 整定辅助函数 内环参数更新
// void tune_cl_param_update(float alpha, float beta)
// {
//     float fc = 1 / slotcon->t_high;

//     float fn_d = 1 / (MATH_2PI * param->motor_ld / param->motor_rs);
//     float wc_d = param->clbw_coef * MATH_2PI * (2 * fn_d < fc / 10 ? 2 *
//     fn_d : fc / 10); foc.PI_id.kp = wc_d * param->motor_ld; foc.PI_id.ki =
//     wc_d * param->motor_rs;

//     float fn_q = 1 / (MATH_2PI * param->motor_lq / param->motor_rs);
//     float wc_q = param->clbw_coef * MATH_2PI * (2 * fn_q < fc / 10 ? 2 *
//     fn_q : fc / 10); foc.PI_iq.kp = wc_q * param->motor_lq; foc.PI_iq.ki =
//     wc_q * param->motor_rs;
// }
static inline void current_process(uint8_t sec, float is_in[3],
                                   float is_out[3]) {
    if (sec == 1 || sec == 6) { // 最短相=W
        is_out[0] = is_in[1] + is_in[2];
        is_out[1] = -is_in[1];
        is_out[2] = -is_in[2];
    } else if (sec == 2 || sec == 3) { // 最短相=U
        is_out[0] = -is_in[0];
        is_out[1] = is_in[0] + is_in[2];
        is_out[2] = -is_in[2];
    } else if (sec == 4 || sec == 5) { // 最短相=V
        is_out[0] = -is_in[0];
        is_out[1] = -is_in[1];
        is_out[2] = is_in[0] + is_in[1];
    } else { // sec 0/7: 零矢量
        is_out[0] = is_in[0];
        is_out[1] = is_in[1];
        is_out[2] = is_in[2];
    }

    is_out[0] = filter_first_order_lag(&core.cf_u, is_out[0]);
    is_out[1] = filter_first_order_lag(&core.cf_v, is_out[1]);
    is_out[2] = filter_first_order_lag(&core.cf_w, is_out[2]);
}

// 核心主循环任务
void core_mainloop_tasks(void) {
    core.obs_running = obs_loop_task(&core.obs); // 观测器任务

    // TODO:通讯任务
    //  更新电流采集和读取电流值、电压值、温度值
    sample_update(&g_sample, !core.enable);
    sample_get_current(&g_sample, core.fb.ims);
    core.fb.udc = sample_get_vbus(&g_sample);
    core.fb.vmax = core.fb.udc * XMATH_INSQRT3;
    core.fb.temp = sample_get_temperature(&g_sample);
    obs_loop_task(&core.obs);
}

// 时间槽高频任务
void obs_foc_task(float ts) {
    // 观测器运行
    obs_tim_task(&core.obs,
                 ts,
                 &core.fb.theta_rotor,
                 &core.fb.theta_elec,
                 &core.fb.vel,
                 &core.fb.pos);
    if (core.state == CORE_OPENLOOP) {
        core.ref.theta_elec += core.ref.vel_elec * ts;
        core.ref.theta_elec = xmath_normalize_angle_2pi(core.ref.theta_elec);
    } else {
        core.ref.theta_elec = core.fb.theta_elec;
    }
    // 预计算sin cos
    xmath_sin_cos_rad_f32(core.ref.theta_elec, &core.fb.sin_e, &core.fb.cos_e);

    // 电流处理
    current_process(core.svpwm.sector, core.fb.ims, core.fb.is);
    // Clarke 变换
    clarke_transform(core.fb.is[0],
                     core.fb.is[1],
                     core.fb.is[2],
                     &core.fb.ialpha,
                     &core.fb.ibeta);
    // Park 变换
    park_transform(core.fb.ialpha,
                   core.fb.ibeta,
                   core.fb.sin_e,
                   core.fb.cos_e,
                   &core.fb.id,
                   &core.fb.iq);

    if (core.enable) {
        // foc电流环更新
        if (core.oc_enable) {
            core.fb.ud = core.ref.duty * core.fb.vmax;
            core.fb.uq = 0;
        } else
            foc_cl_update(&core.ctrl,
                          core.ref.iq,
                          core.fb.iq,
                          core.ref.id,
                          core.fb.id,
                          &core.fb.uq,
                          &core.fb.ud);

        inv_park_transform(core.fb.ud,
                           core.fb.uq,
                           core.fb.sin_e,
                           core.fb.cos_e,
                           &core.fb.ualpha,
                           &core.fb.ubeta);

        // svpwm 调制生成脉冲
        svpwm_update(&core.svpwm, core.fb.ualpha, core.fb.ubeta);
    }
}

// TODO:参数校准任务

// 运行轨迹规划器
void traj_task(float ts) {
    // 电流模式 整定 其他状态不需要轨迹规划
    if (core.state <= CORE_CURRENT_MODE)
        return;
    traj_Update(&core.traj, ts);
    if (core.traj.cfg.mode == TRAJ_DISABLE)
        return;
    if (core.state == CORE_MIT_MODE) {
        // 使用内置轨迹规划器 输出轨迹信息
        core.ref.pos = core.traj.out.value;
        core.ref.vel = core.traj.out.rate_d1;
        core.ref.tau_ff = core.param->motor_j * core.traj.out.rate_d2 +
                          core.param->motor_b * core.traj.out.rate_d1;
    } else if (core.state == CORE_PID_SPEED)
        core.ref.vel = core.traj.out.value;
    else if (core.state == CORE_PID_POSITION) {
        core.ref.pos = core.traj.out.value;
    }
}

// 跑pid速度环/mit
void vl_pid_mit_task(float ts) {
    if (core.state == CORE_PID_SPEED) { // 速度环pid控制 或 位置环
        core.ref.iq = pid_vl_update(&core.ctrl, core.ref.vel, core.fb.vel);
    } else if (core.state == CORE_PID_POSITION) {
        if (core.ctrl.pos_pvc == POS_PID_PVC)
            core.ref.iq = pid_vl_update(&core.ctrl, core.ref.vel, core.fb.vel);
        else
            core.ref.iq = pid_pl_update(&core.ctrl, core.ref.pos, core.fb.pos);
    } else if (core.state == CORE_MIT_MODE) {
        core.ref.tau = mit_con_update(&core.ctrl,
                                      core.ref.tau_ff,
                                      core.ref.pos,
                                      core.fb.pos,
                                      core.ref.vel,
                                      core.fb.vel);

        core.ref.iq = core.ref.tau / core.param->motor_ke;
    }
}
// 弱磁控制
void field_weakening_task(float ts) {
    if (core.state == CORE_PID_SPEED) {
        float vout;
        arm_sqrt_f32((core.fb.ud * core.fb.ud + core.fb.uq * core.fb.uq),
                     &vout);
        float error = core.fb.vmax - vout;
        if (error < 0)
            core.ref.id =
                field_weakening_update(&core.ctrl, core.fb.vmax, vout);
        else
            core.ref.id = 0.0f;
    }
}

// pid位置环
void pl_pid_task(float ts) {
    if (core.state == CORE_PID_POSITION &&
        core.ctrl.pos_pvc == POS_PID_PVC) { // 位置环pid pvc控制
        core.ref.vel = pid_pl_update(&core.ctrl, core.ref.pos, core.fb.pos);
    }
}
