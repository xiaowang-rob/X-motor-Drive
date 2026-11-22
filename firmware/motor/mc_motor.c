#include "mc_motor.h"

#include "slot_con.h"
#include "xmath.h"

tMotorCtrl g_motor;

void motor_init(tParameter *param, tMotorOps *ops) {
    if (!param || !ops)
        return;

    g_motor.ops = ops;
    g_motor.param = param;
    g_motor.ops->gate_drv_enable(false);

    g_motor.state = MOTOR_IDLE;
    return;
}

void motor_reset() {
    // TODO:
    g_motor.state = MOTOR_IDLE;
}

// 失能 释放电机
void motor_disable() {
    g_motor.ops->gate_drv_enable(false);
    motor_reset();
}

void motor_handle_fault() {
    motor_disable();
    g_motor.state = MOTOR_FAULT;
}

static bool vel_open_ctrl = false;
bool motor_cmd_cfg(tCmdCfg *cfg) {
    if (cfg->mode < MOTOR_OPENLOOP || cfg->mode > MOTOR_MIT_MODE)
        return false;
    if (g_motor.state != MOTOR_IDLE)
        return false;

    g_motor.state = cfg->mode;

    switch (cfg->mode) {
    case MOTOR_OPENLOOP:
        g_motor.foc.cl_enable = false;
        g_motor.foc.to_enable = true;
        vel_open_ctrl = cfg->vo_ctrl;
        break;
    case MOTOR_CURRENT_MODE:
        g_motor.foc.cl_enable = true;
        g_motor.ctrl.tau_enable = cfg->tau_ctrl;
        break;
    case MOTOR_SPEED_MODE:
        g_motor.foc.cl_enable = true;
        g_motor.foc.fw_enable = cfg->fw_ctrl;
        break;
    case MOTOR_POSITION_MODE:
        g_motor.foc.cl_enable = true;
        g_motor.ctrl.pvt_enable = cfg->pvt_ctrl;
        break;
    case MOTOR_MIT_MODE:
        g_motor.foc.cl_enable = true;
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
        if (vel_open_ctrl)
            g_motor.foc.ref.vel_elec = cmd->payload.openloop.value;
        else {
            g_motor.foc.ref.theta_e = cmd->payload.openloop.value;
            g_motor.foc.ref.vel_elec = 0;
        }
        g_motor.foc.out.duty_d = XCLAMP(cmd->payload.openloop.duty, 0, 1);
    case MOTOR_CURRENT_MODE:
        if (g_motor.cfg.tau_ctrl) {
            g_motor.ref.tau = cmd->current_ctrl.value;
            g_motor.ref.iq = g_motor.ref.tau / g_motor.param->motor_ke;
        } else {
            g_motor.ref.iq = cmd->current_ctrl.value;
        }

        break;
    case MOTOR_SPEED_MODE:
        traj_set_target(&g_motor.traj, cmd->vel_pid_ctrl.vel, g_motor.fb.vel);
        break;
    case MOTOR_POSITION_MODE:
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

void motor_loop_task() {
    // 更新电机温度
    g_motor.fb.motor_temp = g_motor.ops->get_motor_temp();
    // TODO：状态机
}

// 时间槽高频任务
void motor_foc_tim_task(float ts) {

    // 获取电流
    g_motor.ops->get_current(g_motor.fb.ims);
    // 电流处理 采用上一个周期的电角度解算
    mc_foc_current_process(&g_motor.foc, g_motor.fb.ims);

    // 角度、速度、位置更新
    g_motor.ops->get_theta(&g_motor.sen.theta_r_raw);
    mc_sensor_theta_process(&g_motor.sen, ts, g_motor.param->motor_polepairs);

    // foc 更新 dutyd dutyq
    mc_foc_update(&g_motor.foc, ts, g_motor.sen.theta_e);

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
    mc_foc_svpwm_output(&g_motor.foc);
    // TODO: 这里做死区补偿

    //  输出
    g_motor.ops->gate_drv_out(
        g_motor.foc.out.dutyU, g_motor.foc.out.dutyV, g_motor.foc.out.dutyW);
    // 采样点重置
    g_motor.ops->sp_calibration(g_motor.foc.ref.sec);
}

// 运行指令处理
void motor_cmd_tim_task(float ts) {
    ctrl_cmd_process(&g_motor.ctrl, g_motor.param, ts);
}

// 运行控制器配置
void motor_ctrl_cfg_tim_task(float ts) {
    ctrl_config(&g_motor.ctrl, g_motor.param, ts);
}

// 控制任务
void motor_ctrl_tim_task(float ts) {
    ctrl_update(&g_motor.ctrl, g_motor.param, ts);
}
// 弱磁控制
void motor_field_weakening_tim_task(float ts) {
    field_weakening_update(&g_motor.foc);
}

const tSlotTaskItem high_schedule[FREQ_HIGH_LOOP] = {
    {
        0,
        motor_foc_tim_task,

    },
};
const tSlotTaskItem medium_schedule[FREQ_MEDIUM_LOOP] = {
    {
        0,
        motor_cmd_tim_task,
    },
    {
        1,
        motor_ctrl_cfg_tim_task,
    },
    {
        2,
        motor_ctrl_tim_task,
    },
    {
        3,
        motor_field_weakening_tim_task,
    },
};

const tSlotTaskItem low_schedule[FREQ_LOW_LOOP];
