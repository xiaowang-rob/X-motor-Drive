#include "core.h"

#include "bsp_cfg.h"
#include "bsp_irq.h"
#include "bsp_math.h"

#include "encoder.h"
#include "bus_com.h"
#include "sense.h"

#include "foc.h"
#include "obs.h"
#include "svpwm.h"
#include "trajectory.h"

#include "protection.h"
#include "slot_con.h"
#include "status_feedback.h"

tCore core; // 控制核心
tFOC foc;
tObs obs;
tSvpwm svpwm;

static tFirstOrderLagFilter cf_u, cf_v, cf_w; // 电流滤波器
static tTraj traj;

void current_filter_init(float cfalpha)
{
    filter_first_order_lag_init(&cf_u, cfalpha, 0);
    filter_first_order_lag_init(&cf_v, cfalpha, 0);
    filter_first_order_lag_init(&cf_w, cfalpha, 0);
}

void core_init(void)
{
    // 初始化slot槽服务
    slot_con_init(F_PWM);

    memset(&core.fb, 0, sizeof(core.fb));

    core.enable = false;
    core.state = INIT;

    // 初始化保护服务
    pro_manager_init(&g_param);

    core.fb.udc = sense_get_vbus(&g_sense);
    core.fb.vmax = core.fb.udc * MATH_INSQRT3;

    // 使能栅极驱动
    gate_drv_power_on(&g_gate, true);
    gate_drv_enable(&g_gate, true);
    // 初始化svpwm
    svpwm_init(&svpwm, TIC_PWM, core.fb.udc, T_PWM, T_SAMPLE, T_NOISE, T_DIED);

    // foc初始化
    foc_init(&foc, &g_param, g_slotcon.t_high, g_slotcon.t_med, g_slotcon.t_low, core.fb.vmax);

    // 初始化观测器
    if (!obs_init(&obs))
    {
        core_enter_fault(FAULT_OBS_INIT);
        return;
    }
    if (obs.type == NONE_OBS)
        core.ot_enable = true; // 无观测器时 开环角度
    // 启动主总线通信
    if (!bus_start(&g_can, g_param.can_id))
    {
        core_enter_fault(FAULT_BUSCOM_INIT);
        return;
    }

    // 初始化 内置轨迹规划
    tTraj_Config cfg = {
        .limit_d1 = g_param.traj_limit_d1,
        .limit_d2 = g_param.traj_limit_d2,
        .limit_d3 = g_param.traj_limit_d3,
        .tolerance = g_param.traj_tolerance,
        .type = g_param.traj_type,
    };
    traj_init(&traj, cfg);

    // 电流滤波初始化
    current_filter_init(g_param.cfalpha);

    // 将solt槽任务注册到pwm下溢中断 以 启动solt槽任务
    pwm_register_callback(NULL, slot_con_update);

    return;
}
static inline void core_reset(void)
{

    filter_first_order_lag_reset(&cf_u, 0);
    filter_first_order_lag_reset(&cf_v, 0);
    filter_first_order_lag_reset(&cf_w, 0);

    memset(&core.ref, 0, sizeof(core.ref));
}
// 核心失能
void core_disable(void)
{
    core.enable = false;
    core.ot_enable = false;
    core.ov_enable = false;
    core_reset();
    core.state = IDLE;
    gate_drv_enable(&g_gate, false);
}
// 核心使能和指令写入
bool core_cmd_set(tCmd *cmd)
{
    // 非法输入
    if (cmd->mode < CURRENT_MODE || cmd->mode > MIT_MODE)
        return false;
    if (core.state == FAULT || core.state == WARNING)
        return false;
    core.state = cmd->mode;
    core.enable = true;
    switch (cmd->mode)
    {
    case CURRENT_MODE:
        core.ref.iq = cmd->iq;
        if (core.ot_enable)
        {
            core.ref.id = cmd->id;
            core.ref.vel_elec = cmd->vel_elec;
        }
        break;
    case PID_SPEED:
        traj_set_target(&traj, cmd->vel);
        break;
    case PID_POSITION:
        if (traj.cfg.type == TRAJ_DISABLE)
        {
            core.ref.pos = cmd->pos;
        }
        else if (traj.cfg.type == TRAJ_PVT)
        {
            traj_pvt_add_target(&traj, cmd->pos, cmd->vel, cmd->tp);
        }
        else
            traj_set_target(&traj, cmd->pos);
        break;
    case MIT_MODE:
        if (traj.cfg.type == TRAJ_DISABLE)
        {
            core.ref.tau_ff = cmd->tau_ff;
            core.ref.vel = cmd->vel;
            core.ref.pos = cmd->pos;
        }
        else if (traj.cfg.type == TRAJ_PVT)
        {
            traj_pvt_add_target(&traj, cmd->pos, cmd->vel, cmd->tp);
        }
        else
            traj_set_target(&traj, cmd->pos);
        break;
    default:
        return false;
    }
    return true;
}
// 开环电压指令输入
void core_ov_cmd_set(float ud, float uq, float theta_e)
{
    core.fb.ud = ud;
    core.fb.uq = uq;
    core.fb.theta_elec = theta_e;
    core.ov_enable = true;
    core.enable = true;
}
// 角度开环指令输入
void core_ot_cmd_set_theta(float theta_e)
{

    core.ref.vel_elec = 0.0f;
    core.fb.theta_elec = theta_e;
    core.ot_enable = true;
    core.enable = true;
}
void core_ot_cmd_set_vel(float vel)
{
    core.ref.vel_elec = vel;
    core.ot_enable = true;
    core.enable = true;
}

// 核心进入错误状态
void core_enter_fault(eFault fault)
{
    core.enable = false;
    core_reset();
    core.state = FAULT;
    core.fault = fault;
    gate_drv_enable(&g_gate, false);
    gate_drv_power_on(&g_gate, false); // 直接电源隔断
}
// 核心进入警告状态
void core_enter_warning(eWarning warning)
{
    core.enable = false;
    core_reset();
    core.state = WARNING;
    core.warning = warning;
    gate_drv_enable(&g_gate, false);
}
// // 整定辅助函数 内环参数更新
// void tune_cl_param_update(float alpha, float beta)
// {
//     float fc = 1 / g_slotcon.t_high;

//     float fn_d = 1 / (MATH_2PI * g_param.motor_ld / g_param.motor_rs);
//     float wc_d = g_param.clbw_coef * MATH_2PI * (2 * fn_d < fc / 10 ? 2 * fn_d : fc / 10);
//     foc.PI_id.kp = wc_d * g_param.motor_ld;
//     foc.PI_id.ki = wc_d * g_param.motor_rs;

//     float fn_q = 1 / (MATH_2PI * g_param.motor_lq / g_param.motor_rs);
//     float wc_q = g_param.clbw_coef * MATH_2PI * (2 * fn_q < fc / 10 ? 2 * fn_q : fc / 10);
//     foc.PI_iq.kp = wc_q * g_param.motor_lq;
//     foc.PI_iq.ki = wc_q * g_param.motor_rs;
// }
static inline void current_process(uint8_t sec, float is_in[3], float is_out[3])
{
    if (sec == 1 || sec == 6)
    { // 最短相=W
        is_out[0] = is_in[1] + is_in[2];
        is_out[1] = -is_in[1];
        is_out[2] = -is_in[2];
    }
    else if (sec == 2 || sec == 3)
    { // 最短相=U
        is_out[0] = -is_in[0];
        is_out[1] = is_in[0] + is_in[2];
        is_out[2] = -is_in[2];
    }
    else if (sec == 4 || sec == 5)
    { // 最短相=V
        is_out[0] = -is_in[0];
        is_out[1] = -is_in[1];
        is_out[2] = is_in[0] + is_in[1];
    }
    else
    { // sec 0/7: 零矢量
        is_out[0] = is_in[0];
        is_out[1] = is_in[1];
        is_out[2] = is_in[2];
    }

    is_out[0] = filter_first_order_lag(&cf_u, is_out[0]);
    is_out[1] = filter_first_order_lag(&cf_v, is_out[1]);
    is_out[2] = filter_first_order_lag(&cf_w, is_out[2]);
}

// 核心主循环任务
void core_mainloop_tasks(void)
{
    core.obs_running = obs_loop_task(&obs); // 观测器任务

    // TODO:通讯任务
    //  更新电流采集和读取电流值、电压值、温度值
    sense_update(&g_sense, !core.enable);
    sense_get_current(&g_sense, core.fb.ims);
    core.fb.udc = sense_get_vbus(&g_sense);
    core.fb.vmax = core.fb.udc * MATH_INSQRT3;
    core.fb.temp = sense_get_temperature(&g_sense);

    status_feedback_main_loop(core.state);
}

// 时间槽高频任务
void obs_foc_task(float ts)
{
    // 观测器运行
    obs_tim_task(&obs, ts,
                 &core.fb.theta_elec, &core.fb.vel, &core.fb.pos);
    if (core.ot_enable)
    {
        core.fb.theta_elec += core.ref.vel_elec * ts;
        core.fb.theta_elec = normalize_angle_2pi(core.fb.theta_elec);
    }
    // 预计算sin cos
    arm_sin_cos_rad_f32(core.fb.theta_elec, &core.fb.sin_e, &core.fb.cos_e);

    // 电流处理
    current_process(svpwm.sector, core.fb.ims, core.fb.is);
    // Clarke 变换
    clarke_transform(core.fb.is[0], core.fb.is[1], core.fb.is[2], &core.fb.ialpha, &core.fb.ibeta);
    // Park 变换
    park_transform(core.fb.ialpha, core.fb.ibeta, core.fb.sin_e, core.fb.cos_e, &core.fb.id, &core.fb.iq);

    if (core.enable)
    {
        // foc电流环更新
        if (!core.ov_enable)
            foc_cl_update(&foc, core.ref.iq, core.fb.iq, core.ref.id, core.fb.id, &core.fb.uq, &core.fb.ud);

        inv_park_transform(core.fb.ud, core.fb.uq, core.fb.sin_e, core.fb.cos_e,
                           &core.fb.ualpha, &core.fb.ubeta);

        // svpwm 调制生成脉冲
        svpwm_update(&svpwm, core.fb.ualpha, core.fb.ubeta);
        // 门极驱动输出
        gate_drv_set_compare(&g_gate, svpwm.ticA, svpwm.ticB, svpwm.ticC);
    }
}

// TODO:参数校准任务

// 运行轨迹规划器
void traj_task(float ts)
{
    if (core.enable)
    {
        // 电流模式不需要轨迹规划
        if (core.state == CURRENT_MODE)
            return;
        traj_Update(&traj, ts);
        if (core.state == MIT_MODE)
        {
            if (traj.cfg.type == TRAJ_DISABLE)
                return;

            // 使用内置轨迹规划器 输出轨迹信息
            core.ref.pos = traj.out.value;
            core.ref.vel = traj.out.rate_d1;
            core.ref.tau_ff = g_param.motor_j * traj.out.rate_d2 + g_param.motor_b * traj.out.rate_d1;
        }
        else if (core.state == PID_SPEED)
            core.ref.vel = traj.out.value;
        else if (core.state == PID_POSITION)
        {
            if (traj.cfg.type == TRAJ_DISABLE)
                return;
            core.ref.pos = traj.out.value;
        }
    }
}

// 跑pid速度环/mit
void vl_pid_mit_task(float ts)
{
    if (core.enable)
    {
        if (core.state == PID_SPEED || core.state == PID_POSITION)
        { // 速度环pid控制 或 位置环
            foc_pidvl_update(&foc, core.ref.vel, core.fb.vel, &core.ref.iq);
        }
        else if (core.state == MIT_MODE)
        {
            foc_mit_update(&foc, core.ref.tau_ff, core.ref.pos, core.fb.pos,
                           core.ref.vel, core.fb.vel, &core.ref.tau);

            core.ref.iq = core.ref.tau / g_param.motor_ke;
        }
    }
}
// 弱磁控制
void weak_mag_task(float ts)
{
    if (core.enable)
    {
        float vout;
        arm_sqrt_f32((core.fb.ud * core.fb.ud + core.fb.uq * core.fb.uq), &vout);
        float error = core.fb.vmax - vout;
        if (error < 0)
            foc_weakmag_update(&foc, core.fb.vmax, vout, &core.ref.id);
        else
            core.ref.id = 0.0f;
    }
}

// pid位置环
void pl_pid_task(float ts)
{
    if (core.enable)
    {
        if (core.state == PID_POSITION)
        { // 位置环pid控制
            foc_pidpl_update(&foc, core.ref.pos, core.fb.pos, &core.ref.vel);
        }
    }
}

const tSlotTask high_schedule[FREQ_HIGH_LOOP] = {
    {0, obs_foc_task} // 内环只有一个 观测器和foc核心任务
};
const tSlotTask medium_schedule[FREQ_MEDIUM_LOOP] = {
    {4, traj_task},
    {5, vl_pid_mit_task},
    {6, weak_mag_task},
};
const tSlotTask low_schedule[FREQ_LOW_LOOP] = {
    {1, pl_pid_task},
};
