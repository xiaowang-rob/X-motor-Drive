#include "motor_ctrl.h"

#include "xmath.h"

tMotorCtrl g_motor;

void motor_init(tParameter *param, tMotorOps *ops, float t_hl, float t_ml,
                float t_ll) {
    if (!param || !ops)
        return;

    g_motor.ops = ops;
    g_motor.param = param;

    memset(&g_motor.fb, 0, sizeof(g_motor.fb));

    g_motor.ops->gate_drv_enable(false);

    // foc初始化
    ctrl_foc_init(&g_motor.ctrl, g_motor.param, param->cfalpha, t_hl);
    // pid控制器初始化
    ctrl_pid_init(&g_motor.ctrl, g_motor.param, t_ml, t_ll);
    // mit控制器初始化
    ctrl_mit_init(&g_motor.ctrl, g_motor.param);

    // 初始化观测器
    if (g_motor.param->sensorless != SENSORLESS_NONE) {
        // TODO:初始化无感算法
        g_motor.obs_on = true;
    }

    // 初始化 内置轨迹规划
    traj_init(&g_motor.traj, g_motor.param);

    motor_reset();

    return;
}

void motor_reset(void) {

    ctrl_reset(&g_motor.ctrl);
    traj_reset(&g_motor.traj);

    memset(&g_motor.ref, 0, sizeof(g_motor.ref));
    memset(&g_motor.cfg, 0, sizeof(g_motor.cfg));

    g_motor.state = MOTOR_IDLE;
}

// 失能 释放电机
void motor_disable(void) {
    g_motor.ops->gate_drv_enable(false);
    motor_reset();
}

void motor_handle_fault(void) {
    motor_disable();
    g_motor.state = MOTOR_FAULT;
}

bool motor_cmd_cfg(tCmdCfg *cfg) {
    if (cfg->mode < MOTOR_OPENLOOP || cfg->mode > MOTOR_MIT_MODE)
        return false;
    if (g_motor.state != MOTOR_IDLE)
        return false;

    g_motor.state = cfg->mode;

    switch (cfg->mode) {
    case MOTOR_OPENLOOP:
        g_motor.cfg.cur_enable = false;
        g_motor.cfg.ol_vel_ctrl = cfg->ol_vel_ctrl;
        break;
    case MOTOR_CURRENT_MODE:
        g_motor.cfg.cur_enable = true;
        g_motor.cfg.tau_ctrl = cfg->tau_ctrl;
        break;
    case MOTOR_PID_SPEED:
        g_motor.cfg.cur_enable = true;
        g_motor.cfg.fw_ctrl = cfg->fw_ctrl;
        break;
    case MOTOR_PID_POSITION:
        g_motor.cfg.cur_enable = true;
        g_motor.cfg.pvt_ctrl = cfg->pvt_ctrl;
        break;
    case MOTOR_MIT_MODE:
        g_motor.cfg.cur_enable = true;
        mit_config(&g_motor.ctrl.mit, cfg->mit_kp, cfg->mit_kd);
        break;
    default:
        return false;
    }
    // 使能前先写入 50% 零矢量，避免 CCR=0 时三相下桥全通的过渡状态
    g_motor.ops->gate_drv_out(0.5f, 0.5f, 0.5f);
    g_motor.ops->gate_drv_enable(true);
    return true;
}
// 核心控制参数写入
bool motor_cmd_set(tCmdValue *cmd) {

    if (cmd->mode != g_motor.state)
        return false;

    switch (cmd->mode) {
    case MOTOR_OPENLOOP:
        if (g_motor.cfg.ol_vel_ctrl)
            g_motor.ref.vel_elec = cmd->openloop.value;
        else {
            g_motor.ref.theta_elec = cmd->openloop.value;
            g_motor.ref.vel_elec = 0;
        }
        g_motor.ref.dutyd = XCLAMP(cmd->openloop.duty, 0, 1);
    case MOTOR_CURRENT_MODE:
        if (g_motor.cfg.tau_ctrl) {
            g_motor.ref.tau = cmd->current_ctrl.value;
            g_motor.ref.iq = g_motor.ref.tau / g_motor.param->motor_ke;
        } else {
            g_motor.ref.iq = cmd->current_ctrl.value;
        }

        break;
    case MOTOR_PID_SPEED:
        traj_set_target(&g_motor.traj, cmd->vel_pid_ctrl.vel, g_motor.fb.vel);
        break;
    case MOTOR_PID_POSITION:
        // TODO:添加pvt处理
        traj_set_target(&g_motor.traj, cmd->pos_pid_ctrl.pos, g_motor.fb.pos);
        break;
    case MOTOR_MIT_MODE:
        if (g_motor.traj.cfg.mode == TRAJ_DISABLE) {
            g_motor.ref.tau_ff = cmd->mit_ctrl.tau_ff;
            g_motor.ref.vel = cmd->mit_ctrl.vel;
            g_motor.ref.pos = cmd->mit_ctrl.pos;
        } else
            traj_set_target(&g_motor.traj, cmd->mit_ctrl.pos, g_motor.fb.pos);
        break;
    default:
        return false;
    }
    return true;
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

// 时间槽高频任务
void motor_foc_tim_task(float ts) {

    // 获取电流
    g_motor.ops->get_current(g_motor.fb.ims);
    // 电流处理 采用上一个周期的电角度解算
    current_process(
        &g_motor.ctrl, g_motor.ref.sec, g_motor.fb.ims, g_motor.fb.is);
    // Clarke 变换
    clarke_transform(g_motor.fb.is[0],
                     g_motor.fb.is[1],
                     g_motor.fb.is[2],
                     &g_motor.fb.ialpha,
                     &g_motor.fb.ibeta);
    // Park 变换
    park_transform(g_motor.fb.ialpha,
                   g_motor.fb.ibeta,
                   g_motor.ref.sin_e,
                   g_motor.ref.cos_e,
                   &g_motor.fb.id,
                   &g_motor.fb.iq);

    // 角度、速度、位置更新
    g_motor.ops->theta_process(
        ts, &g_motor.fb.theta_elec, &g_motor.fb.theta_rotor, &g_motor.fb.vel);
    g_motor.fb.pos = pos_accumulate(&g_motor.pacc, g_motor.fb.theta_rotor);
    if (g_motor.state == MOTOR_OPENLOOP) {
        g_motor.ref.theta_elec += g_motor.ref.vel_elec * ts;
        g_motor.ref.theta_elec = XANGLE_2PI(g_motor.ref.theta_elec);
    } else {
        g_motor.ref.theta_elec = g_motor.fb.theta_elec;
    }
    g_motor.ref.sec = (g_motor.ref.theta_elec / XMATH_PI_3) + 0.5f;

    // 预计算sin cos
    XSIN_COS(g_motor.ref.theta_elec, &g_motor.ref.sin_e, &g_motor.ref.cos_e);

    if (g_motor.state < MOTOR_OPENLOOP)
        return;
    // foc电流环更新
    if (g_motor.cfg.cur_enable)
        foc_cl_update(&g_motor.ctrl,
                      g_motor.ref.id,
                      g_motor.fb.id,
                      g_motor.ref.iq,
                      g_motor.fb.iq,
                      &g_motor.ref.dutyd,
                      &g_motor.ref.dutyq);

    inv_park_transform(g_motor.ref.dutyd,
                       g_motor.ref.dutyq,
                       g_motor.ref.sin_e,
                       g_motor.ref.cos_e,
                       &g_motor.ref.dutya,
                       &g_motor.ref.dutyb);
    // 前面uduq输出是按照 1占空比 对 Vsmax 最大相电压算的 后面得转化为
    // 对母线电压，因为svpwm 调制是对 三相逆变电路而言的
    g_motor.ref.dutya *= XMATH_INSQRT3;
    g_motor.ref.dutyb *= XMATH_INSQRT3;

    // svpwm 调制生成脉冲
    svpwm_update(g_motor.ref.sec,
                 g_motor.ref.dutya,
                 g_motor.ref.dutyb,
                 &g_motor.ref.dutyU,
                 &g_motor.ref.dutyV,
                 &g_motor.ref.dutyW);

    // 输出
    g_motor.ops->gate_drv_out(
        g_motor.ref.dutyU, g_motor.ref.dutyV, g_motor.ref.dutyW);
    // 采样点重置
    g_motor.ops->sp_calibration(g_motor.ref.sec);
}

// 运行轨迹规划器
void motor_traj_tim_task(float ts) {
    // 电流模式 整定 其他状态不需要轨迹规划
    if (g_motor.state <= MOTOR_CURRENT_MODE)
        return;
    traj_Update(&g_motor.traj, ts);

    if (g_motor.state == MOTOR_MIT_MODE) {
        // 使用内置轨迹规划器 输出轨迹信息
        g_motor.ref.pos = g_motor.traj.out.value;
        g_motor.ref.vel = g_motor.traj.out.rate_d1;
        g_motor.ref.tau_ff = g_motor.param->motor_j * g_motor.traj.out.rate_d2 +
                             g_motor.param->motor_b * g_motor.traj.out.rate_d1;
    } else if (g_motor.state == MOTOR_PID_SPEED)
        g_motor.ref.vel = g_motor.traj.out.value;
    else if (g_motor.state == MOTOR_PID_POSITION) {
        g_motor.ref.pos = g_motor.traj.out.value;
    }
}

// 跑pid速度环/mit
void motor_vl_pid_mit_tim_task(float ts) {
    if (g_motor.state == MOTOR_PID_SPEED) { // 速度环pid控制 或 位置环
        g_motor.ref.iq =
            pid_vl_update(&g_motor.ctrl, g_motor.ref.vel, g_motor.fb.vel);
    } else if (g_motor.state == MOTOR_PID_POSITION) {
        if (g_motor.ctrl.pos_pvc == POS_PID_PVC)
            g_motor.ref.iq =
                pid_vl_update(&g_motor.ctrl, g_motor.ref.vel, g_motor.fb.vel);
        else
            g_motor.ref.iq =
                pid_pl_update(&g_motor.ctrl, g_motor.ref.pos, g_motor.fb.pos);
    } else if (g_motor.state == MOTOR_MIT_MODE) {
        g_motor.ref.tau = mit_con_update(&g_motor.ctrl,
                                         g_motor.ref.tau_ff,
                                         g_motor.ref.pos,
                                         g_motor.fb.pos,
                                         g_motor.ref.vel,
                                         g_motor.fb.vel);

        g_motor.ref.iq = g_motor.ref.tau / g_motor.param->motor_ke;
    }
}
// 弱磁控制
void motor_field_weakening_tim_task(float ts) {
    if (g_motor.state == MOTOR_PID_SPEED) {
        if (g_motor.ref.dutyq > 0.9f) {
            float duty_out;
            arm_sqrt_f32((g_motor.ref.dutyd * g_motor.ref.dutyd +
                          g_motor.ref.dutyq * g_motor.ref.dutyq),
                         &duty_out);

            g_motor.ref.id =
                field_weakening_update(&g_motor.ctrl, 1.0f, duty_out);
        } else {
            g_motor.ref.id = 0.0f;
        }
    }
}

// pid位置环
void motor_pl_pid_tim_task(float ts) {
    if (g_motor.state == MOTOR_PID_POSITION &&
        g_motor.ctrl.pos_pvc == POS_PID_PVC) { // 位置环pid pvc控制
        g_motor.ref.vel =
            pid_pl_update(&g_motor.ctrl, g_motor.ref.pos, g_motor.fb.pos);
    }
}
