#include "mc_motor.h"

#include "slot_con.h"
#include "xmath.h"

tMotorCtrl g_motor;

void motor_init(tMotorCtrl *mc, tParameter *param, tMotorOps *ops) {
    if (!param || !ops)
        return;

    mc->ops = ops;
    mc->param = param;

    memset(&mc->fb, 0, sizeof(mc->fb));
    memset(&mc->ref, 0, sizeof(mc->ref));
    memset(&mc->cfg, 0, sizeof(mc->cfg));

    mc->ops->gate_drv_enable(false);

    // 初始化观测器
    if (mc->param->sensorless != SENSORLESS_NONE) {
        // TODO:初始化无感算法

        mc->obs_on = true;
    }

    mc->state = MOTOR_IDLE;
    return;
}

void motor_reset(tMotorCtrl *mc) {

    ctrl_reset(&mc->ctrl);

    memset(&mc->ref, 0, sizeof(mc->ref));
    memset(&mc->cfg, 0, sizeof(mc->cfg));

    mc->state = MOTOR_IDLE;
}

// 失能 释放电机
void motor_disable(tMotorCtrl *mc) {
    mc->ops->gate_drv_enable(false);
    motor_reset();
}

void motor_handle_fault(tMotorCtrl *mc) {
    motor_disable();
    mc->state = MOTOR_FAULT;
}

bool motor_cmd_cfg(tMotorCtrl *mc, tCmdCfg *cfg) {
    if (cfg->mode < MOTOR_OPENLOOP || cfg->mode > MOTOR_MIT_MODE)
        return false;
    if (mc->state != MOTOR_IDLE)
        return false;

    mc->state = cfg->mode;

    switch (cfg->mode) {
    case MOTOR_OPENLOOP:
        mc->cfg.cur_enable = false;
        mc->cfg.ol_vel_ctrl = cfg->ol_vel_ctrl;
        break;
    case MOTOR_CURRENT_MODE:
        mc->cfg.cur_enable = true;
        mc->cfg.tau_ctrl = cfg->tau_ctrl;
        break;
    case MOTOR_SPEED_MODE:
        mc->cfg.cur_enable = true;
        mc->cfg.fw_ctrl = cfg->fw_ctrl;
        break;
    case MOTOR_POSITION_MODE:
        mc->cfg.cur_enable = true;
        mc->cfg.pvt_ctrl = cfg->pvt_ctrl;
        break;
    case MOTOR_MIT_MODE:
        mc->cfg.cur_enable = true;
        mit_config(&mc->ctrl.mit, cfg->mit_kp, cfg->mit_kd);
        break;
    default:
        return false;
    }
    // 使能前先写入 50% 零矢量，避免 CCR=0 时三相下桥全通的过渡状态
    mc->ops->gate_drv_out(0.5f, 0.5f, 0.5f);
    mc->ops->gate_drv_enable(true);
    return true;
}
// 核心控制参数写入
bool motor_cmd_set(tMotorCtrl *mc, tCmdValue *cmd) {

    if (cmd->mode != mc->state)
        return false;

    switch (cmd->mode) {
    case MOTOR_OPENLOOP:
        if (mc->cfg.ol_vel_ctrl)
            mc->ref.vel_elec = cmd->openloop.value;
        else {
            mc->foc.ref.theta_e = cmd->openloop.value;
            mc->ref.vel_elec = 0;
        }
        mc->ref.dutyd = XCLAMP(cmd->openloop.duty, 0, 1);
    case MOTOR_CURRENT_MODE:
        if (mc->cfg.tau_ctrl) {
            mc->ref.tau = cmd->current_ctrl.value;
            mc->ref.iq = mc->ref.tau / mc->param->motor_ke;
        } else {
            mc->ref.iq = cmd->current_ctrl.value;
        }

        break;
    case MOTOR_SPEED_MODE:
        traj_set_target(&mc->traj, cmd->vel_pid_ctrl.vel, mc->fb.vel);
        break;
    case MOTOR_POSITION_MODE:
        // TODO:添加pvt处理
        traj_set_target(&mc->traj, cmd->pos_pid_ctrl.pos, mc->fb.pos);
        break;
    case MOTOR_MIT_MODE:
        if (mc->traj.cfg.mode == TRAJ_DISABLE) {
            mc->ref.tau_ff = cmd->mit_ctrl.tau_ff;
            mc->ref.vel = cmd->mit_ctrl.vel;
            mc->ref.pos = cmd->mit_ctrl.pos;
        } else
            traj_set_target(&mc->traj, cmd->mit_ctrl.pos, mc->fb.pos);
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

void motor_loop_task(tMotorCtrl *mc) {
    // 更新电机温度
    mc->fb.motor_temp = mc->ops->get_motor_temp();
    // TODO：状态机
}

// 时间槽高频任务
void motor_foc_tim_task(tMotorCtrl *mc, float ts) {

    // 获取电流
    mc->ops->get_current(mc->fb.ims);
    // 电流处理 采用上一个周期的电角度解算
    mc_foc_current_process(&mc->foc, mc->fb.ims);

    // 角度、速度、位置更新
    mc->ops->theta_process(
        ts, &mc->fb.theta_elec, &mc->fb.theta_rotor, &mc->fb.vel, &mc->fb.pos);

    if (mc->state == MOTOR_OPENLOOP) {
        mc->foc.ref.theta_e += mc->ref.vel_elec * ts;
        mc->foc.ref.theta_e = XANGLE_2PI(mc->foc.ref.theta_e);
    } else {
        mc->foc.ref.theta_e = mc->fb.theta_elec;
    }
    mc->ref.sec = (mc->foc.ref.theta_e / XMATH_PI_3) + 0.5f;

    // 预计算sin cos
    XSIN_COS(mc->foc.ref.theta_e, &mc->foc.ref.sin_e, &mc->foc.ref.cos_e);

    if (mc->state < MOTOR_OPENLOOP)
        return;
    // foc电流环更新 更新 dutyd dutyq
    mc_foc_cl_update(&mc->foc);

    // TODO:电压补偿算法,和d轴高频注入都可以在这里进行
    /*
    if (decoupling_enabled) {
        // 交叉耦合补偿
        dec_vd = iq * omega_e * Lq;               // +ωe·Lq·iq
        dec_vq = -id * omega_e * Ld;              // -ωe·Ld·id

        // 反电动势补偿（通常与交叉耦合一起使能）
        dec_vq -= omega_e * flux_linkage;         // -ωe·ψf
    }
    */
    // svpwm 调制生成脉冲
    mc_foc_svpwm_output(&mc->foc);
    // TODO: 这里做死区补偿

    //  输出
    mc->ops->gate_drv_out(mc->ref.dutyU, mc->ref.dutyV, mc->ref.dutyW);
    // 采样点重置
    mc->ops->sp_calibration(mc->ref.sec);
}

// 运行轨迹规划器
void motor_traj_tim_task(tMotorCtrl *mc, float ts) {
    // 电流模式 整定 其他状态不需要轨迹规划
    if (mc->state <= MOTOR_CURRENT_MODE)
        return;
    traj_Update(&mc->traj, ts);

    if (mc->state == MOTOR_MIT_MODE) {
        // 使用内置轨迹规划器 输出轨迹信息
        mc->ref.pos = mc->traj.out.value;
        mc->ref.vel = mc->traj.out.rate_d1;
        mc->ref.tau_ff = mc->param->motor_j * mc->traj.out.rate_d2 +
                         mc->param->motor_b * mc->traj.out.rate_d1;
    } else if (mc->state == MOTOR_SPEED_MODE)
        mc->ref.vel = mc->traj.out.value;
    else if (mc->state == MOTOR_POSITION_MODE) {
        mc->ref.pos = mc->traj.out.value;
    }
}

// 跑pid速度环/mit
void motor_vl_pid_mit_tim_task(tMotorCtrl *mc, float ts) {
    if (mc->state == MOTOR_SPEED_MODE) { // 速度环pid控制 或 位置环
        mc->ref.iq = pid_vl_update(&mc->ctrl, mc->ref.vel, mc->fb.vel);
    } else if (mc->state == MOTOR_POSITION_MODE) {

        if (mc->param->pos_pidmode == POS_PID_PVC)
            mc->ref.iq = pid_vl_update(&mc->ctrl, mc->ref.vel, mc->fb.vel);
        else
            mc->ref.iq = pid_pl_update(&mc->ctrl, mc->ref.pos, mc->fb.pos);
    } else if (mc->state == MOTOR_MIT_MODE) {

        mc->ref.tau = mit_con_update(&mc->ctrl,
                                     mc->ref.tau_ff,
                                     mc->ref.pos,
                                     mc->fb.pos,
                                     mc->ref.vel,
                                     mc->fb.vel);

        mc->ref.iq = mc->ref.tau / mc->param->motor_ke;
    }
}
// 弱磁控制
void motor_field_weakening_tim_task(tMotorCtrl *mc, float ts) {
    if (mc->cfg.fw_ctrl) {
        if (mc->ref.dutyq > 0.9f) {
            mc->ref.duty = XSQRTF(mc->ref.dutyd * mc->ref.dutyd +
                                  mc->ref.dutyq * mc->ref.dutyq);

            mc->ref.id = field_weakening_update(&mc->ctrl, 1.0f, mc->ref.duty);
        } else {
            mc->ref.duty = mc->ref.dutyq;
            mc->ref.id = 0.0f;
        }
    }
}

// pid位置环
void motor_pl_pid_tim_task(tMotorCtrl *mc, float ts) {
    if (mc->state == MOTOR_POSITION_MODE &&
        mc->param->pos_pidmode == POS_PID_PVC) { // 位置环pid pvc控制
        mc->ref.vel = pid_pl_update(&mc->ctrl, mc->ref.pos, mc->fb.pos);
    }
}

const tSlotTaskItem high_schedule[FREQ_HIGH_LOOP] = {
    {
        0,
        ,
    },
};
const tSlotTaskItem medium_schedule[FREQ_MEDIUM_LOOP];
const tSlotTaskItem low_schedule[FREQ_LOW_LOOP];
