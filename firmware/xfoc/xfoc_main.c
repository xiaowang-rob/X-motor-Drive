#include "xfoc_main.h"

#include "xmath.h"

#include "encoder.h"
#include "pwm.h"
#include "sample.h"

tXFocMotor motor;

void current_filter_init(float cfalpha) {
    filter_first_order_lag_init(&motor.cf_u, cfalpha, 0);
    filter_first_order_lag_init(&motor.cf_v, cfalpha, 0);
    filter_first_order_lag_init(&motor.cf_w, cfalpha, 0);
}

void motor_init(tParameter *param, float t_hl, float t_ml, float t_ll) {

    memset(&motor.fb, 0, sizeof(motor.fb));

    motor.state = MOTOR_INIT;
    motor.param = param;
    motor.fb.udc = sample_get_vbus(&g_sample);
    motor.fb.vmax = motor.fb.udc * XMATH_INSQRT3;

    // 使能栅极驱动
    pwm_power_on(&g_pwm, true);
    pwm_enable(&g_pwm, true);
    // 初始化svpwm
    svpwm_init(&motor.svpwm, &g_pwm, motor.fb.udc);

    // foc初始化
    ctrl_foc_init(&motor.ctrl, motor.param, t_hl, motor.fb.vmax);
    // pid控制器初始化
    ctrl_pid_init(&motor.ctrl, motor.param, t_ml, t_ll);
    // mit控制器初始化
    ctrl_mit_init(&motor.ctrl, motor.param);

    // 初始化观测器
    if (!obs_init(&motor.obs, motor.param)) {
        // TODO:观测器换位置
        return;
    }

    // 初始化 内置轨迹规划
    traj_init(&motor.traj, motor.param);

    // 电流滤波初始化
    current_filter_init(param->cfalpha);

    motor_reset();

    return;
}

void motor_reset(void) {

    filter_first_order_lag_reset(&motor.cf_u, 0);
    filter_first_order_lag_reset(&motor.cf_v, 0);
    filter_first_order_lag_reset(&motor.cf_w, 0);

    ctrl_reset(&motor.ctrl);
    traj_reset(&motor.traj);

    memset(&motor.ref, 0, sizeof(motor.ref));
    memset(&motor.cfg, 0, sizeof(motor.cfg));

    motor.state = MOTOR_IDLE;
}

// 失能 释放电机
void motor_disable(void) {
    pwm_enable(&g_pwm, false);
    motor_reset();
}

void motor_handle_fault(void) {
    motor_disable();
    motor.state = MOTOR_FAULT;
    pwm_power_on(&g_pwm, false); // 直接电源隔断
}

bool motor_cmd_cfg(tCmdCfg *cfg) {
    if (cfg->mode < MOTOR_OPENLOOP || cfg->mode > MOTOR_MIT_MODE)
        return false;
    if (motor.state != MOTOR_IDLE)
        return false;

    motor.state = cfg->mode;

    switch (cfg->mode) {
    case MOTOR_OPENLOOP:
        motor.cfg.cur_enable = false;
        motor.cfg.ol_vel_ctrl = cfg->ol_vel_ctrl;
        break;
    case MOTOR_CURRENT_MODE:
        motor.cfg.cur_enable = true;
        motor.cfg.tau_ctrl = cfg->tau_ctrl;
        break;
    case MOTOR_PID_SPEED:
        motor.cfg.cur_enable = true;
        motor.cfg.fw_ctrl = cfg->fw_ctrl;
        break;
    case MOTOR_PID_POSITION:
        motor.cfg.cur_enable = true;
        motor.cfg.pvt_ctrl = cfg->pvt_ctrl;
        break;
    case MOTOR_MIT_MODE:
        motor.cfg.cur_enable = true;
        mit_config(&motor.ctrl.mit, cfg->mit_kp, cfg->mit_kd);
        break;
    default:
        return false;
    }
    pwm_power_on(&g_pwm, true);
    pwm_enable(&g_pwm, true);
    return true;
}
// 核心控制参数写入
bool motor_cmd_set(tCmdValue *cmd) {

    if (cmd->mode != motor.state)
        return false;

    switch (cmd->mode) {
    case MOTOR_OPENLOOP:
        if (motor.cfg.ol_vel_ctrl)
            motor.ref.vel_elec = cmd->openloop.value;
        else {
            motor.ref.theta_elec = cmd->openloop.value;
            motor.ref.vel_elec = 0;
        }
        motor.ref.duty = XCLAMP(cmd->openloop.duty, 0, 1);
    case MOTOR_CURRENT_MODE:
        if (motor.cfg.tau_ctrl) {
            motor.ref.tau = cmd->current_ctrl.value;
            motor.ref.iq = motor.ref.tau / motor.param->motor_ke;
        } else {
            motor.ref.iq = cmd->current_ctrl.value;
        }

        break;
    case MOTOR_PID_SPEED:
        traj_set_target(&motor.traj, cmd->vel_pid_ctrl.vel, motor.fb.vel);
        break;
    case MOTOR_PID_POSITION:
        // TODO:添加pvt处理
        traj_set_target(&motor.traj, cmd->pos_pid_ctrl.pos, motor.fb.pos);
        break;
    case MOTOR_MIT_MODE:
        if (motor.traj.cfg.mode == TRAJ_DISABLE) {
            motor.ref.tau_ff = cmd->mit_ctrl.tau_ff;
            motor.ref.vel = cmd->mit_ctrl.vel;
            motor.ref.pos = cmd->mit_ctrl.pos;
        } else
            traj_set_target(&motor.traj, cmd->mit_ctrl.pos, motor.fb.pos);
        break;
    default:
        return false;
    }
    return true;
}

// 设置零点
bool motor_set_pos_zero(void) {
    if (motor.state != MOTOR_IDLE) {
        return false;
    }
    obs_set_zero(&motor.obs);
}
// 设置限位
bool motor_set_pos_limit(void) {
    if (motor.state != MOTOR_IDLE) {
        return false;
    }
    obs_set_pos_limit(&motor.obs);
}
// // 整定辅助函数 内环参数更新
// void tune_cl_param_update(float alpha, float beta)
// {
//     float fc = 1 / slotcon->t_high;

//     float fn_d = 1 / (MATH_2PI * param->motor_ld / param->motor_rs);
//     float wc_d = param->clbw_coef * MATH_2PI * (2 * fn_d < fc / 10 ? 2 *
//     fn_d : fc / 10); foc.PI_id.kp = wc_d * param->motor_ld; foc.PI_id.ki
//     = wc_d * param->motor_rs;

//     float fn_q = 1 / (MATH_2PI * param->motor_lq / param->motor_rs);
//     float wc_q = param->clbw_coef * MATH_2PI * (2 * fn_q < fc / 10 ? 2 *
//     fn_q : fc / 10); foc.PI_iq.kp = wc_q * param->motor_lq; foc.PI_iq.ki
//     = wc_q * param->motor_rs;
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

    is_out[0] = filter_first_order_lag(&motor.cf_u, is_out[0]);
    is_out[1] = filter_first_order_lag(&motor.cf_v, is_out[1]);
    is_out[2] = filter_first_order_lag(&motor.cf_w, is_out[2]);
}

// 核心主循环任务
void motor_mainloop_tasks(void) {
    obs_loop_task(&motor.obs); // 观测器任务

    //  更新电流采集和读取电流值、电压值、温度值
    sample_update(&g_sample, !motor.cfg.cur_enable);
    sample_get_current(&g_sample, motor.fb.ims);
    motor.fb.udc = sample_get_vbus(&g_sample);
    motor.fb.vmax = motor.fb.udc * XMATH_INSQRT3;
    motor.fb.temp = sample_get_temperature(&g_sample);
}

// 时间槽高频任务
void obs_foc_task(float ts) {

    // 观测器运行
    obs_tim_task(&motor.obs,
                 ts,
                 &motor.fb.theta_rotor,
                 &motor.fb.theta_elec,
                 &motor.fb.vel,
                 &motor.fb.pos);
    if (motor.state == MOTOR_OPENLOOP) {
        motor.ref.theta_elec += motor.ref.vel_elec * ts;
        motor.ref.theta_elec = xmath_normalize_angle_2pi(motor.ref.theta_elec);
    } else {
        motor.ref.theta_elec = motor.fb.theta_elec;
    }
    // 预计算sin cos
    xmath_sin_cos_rad_f32(
        motor.ref.theta_elec, &motor.fb.sin_e, &motor.fb.cos_e);

    // 电流处理
    current_process(motor.svpwm.sector, motor.fb.ims, motor.fb.is);
    // Clarke 变换
    clarke_transform(motor.fb.is[0],
                     motor.fb.is[1],
                     motor.fb.is[2],
                     &motor.fb.ialpha,
                     &motor.fb.ibeta);
    // Park 变换
    park_transform(motor.fb.ialpha,
                   motor.fb.ibeta,
                   motor.fb.sin_e,
                   motor.fb.cos_e,
                   &motor.fb.id,
                   &motor.fb.iq);

    // foc电流环更新
    if (!motor.cfg.cur_enable) {
        motor.fb.ud = motor.ref.duty * motor.fb.vmax;
        motor.fb.uq = 0;
    } else
        foc_cl_update(&motor.ctrl,
                      motor.ref.iq,
                      motor.fb.iq,
                      motor.ref.id,
                      motor.fb.id,
                      &motor.fb.uq,
                      &motor.fb.ud);

    inv_park_transform(motor.fb.ud,
                       motor.fb.uq,
                       motor.fb.sin_e,
                       motor.fb.cos_e,
                       &motor.fb.ualpha,
                       &motor.fb.ubeta);

    // svpwm 调制生成脉冲
    svpwm_update(&motor.svpwm, motor.fb.ualpha, motor.fb.ubeta);
}

// 运行轨迹规划器
void traj_task(float ts) {
    // 电流模式 整定 其他状态不需要轨迹规划
    if (motor.state <= MOTOR_CURRENT_MODE)
        return;
    traj_Update(&motor.traj, ts);

    if (motor.state == MOTOR_MIT_MODE) {
        // 使用内置轨迹规划器 输出轨迹信息
        motor.ref.pos = motor.traj.out.value;
        motor.ref.vel = motor.traj.out.rate_d1;
        motor.ref.tau_ff = motor.param->motor_j * motor.traj.out.rate_d2 +
                           motor.param->motor_b * motor.traj.out.rate_d1;
    } else if (motor.state == MOTOR_PID_SPEED)
        motor.ref.vel = motor.traj.out.value;
    else if (motor.state == MOTOR_PID_POSITION) {
        motor.ref.pos = motor.traj.out.value;
    }
}

// 跑pid速度环/mit
void vl_pid_mit_task(float ts) {
    if (motor.state == MOTOR_PID_SPEED) { // 速度环pid控制 或 位置环
        motor.ref.iq = pid_vl_update(&motor.ctrl, motor.ref.vel, motor.fb.vel);
    } else if (motor.state == MOTOR_PID_POSITION) {
        if (motor.ctrl.pos_pvc == POS_PID_PVC)
            motor.ref.iq =
                pid_vl_update(&motor.ctrl, motor.ref.vel, motor.fb.vel);
        else
            motor.ref.iq =
                pid_pl_update(&motor.ctrl, motor.ref.pos, motor.fb.pos);
    } else if (motor.state == MOTOR_MIT_MODE) {
        motor.ref.tau = mit_con_update(&motor.ctrl,
                                       motor.ref.tau_ff,
                                       motor.ref.pos,
                                       motor.fb.pos,
                                       motor.ref.vel,
                                       motor.fb.vel);

        motor.ref.iq = motor.ref.tau / motor.param->motor_ke;
    }
}
// 弱磁控制
void field_weakening_task(float ts) {
    if (motor.state == MOTOR_PID_SPEED) {
        float vout;
        arm_sqrt_f32((motor.fb.ud * motor.fb.ud + motor.fb.uq * motor.fb.uq),
                     &vout);
        float error = motor.fb.vmax - vout;
        if (error < 0)
            motor.ref.id =
                field_weakening_update(&motor.ctrl, motor.fb.vmax, vout);
        else
            motor.ref.id = 0.0f;
    }
}

// pid位置环
void pl_pid_task(float ts) {
    if (motor.state == MOTOR_PID_POSITION &&
        motor.ctrl.pos_pvc == POS_PID_PVC) { // 位置环pid pvc控制
        motor.ref.vel = pid_pl_update(&motor.ctrl, motor.ref.pos, motor.fb.pos);
    }
}
