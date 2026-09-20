#include "core.h"

#include "foc.h"
#include "svpwm.h"

#include "bsp_cfg.h"
#include "encoder.h"
#include "pll.h"
#include "trajectory.h"

#include "slot_con.h"

tCore core;
tFOC foc;
tSvpwm svpwm;

static tPLL enc_pll;
static tTraj traj;

// 控制外环初始化 PI速度环 pid位置环 mit控制环 t_vl:速度环周期 t_pl:位置环周期
static void core_loop_init(tCore *core, tParameter *param, float t_vl, float t_pl)
{
    pi_init(&core->PI_vel, param->vlkp, param->vlki, param->limit_current, t_vl);
    pi_init(&core->PI_weakmag, param->vlkp / 2, param->vlki / 2, param->limit_current, t_vl);
    pid_init(&core->PID_pos, param->plkp, param->plki, param->plkd, param->limit_vel, param->plalpha, t_pl);
    mit_init(&core->mit, param->mit_kp, param->mit_kd, param->mit_tsta, param->mit_tmax);
}
// 控制外环复位 PI速度环 pid位置环复位 mit/pid指令值归零
static void core_loop_reset(tCore *core)
{
    pi_reset(&core->PI_vel);
    pi_reset(&core->PI_weakmag);
    pid_reset(&core->PID_pos);

    memset(&core->pidtag, 0, sizeof(tPIDtarget));
    memset(&core->mittag, 0, sizeof(tMITtarget));
}

// 位置累积
static void pos_accumulate(tCore *core)
{
    float angle_delta = core->val.theta_mech - core->pacc.last_angle;
    if (angle_delta < -MATH_PI)
        core->pacc.num_turns++;
    else if (angle_delta > MATH_PI)
        core->pacc.num_turns--;
    core->pacc.last_angle = core->val.theta_mech;
    core->val.pos = (core->val.theta_mech - core->pacc.zero_angle) + core->pacc.num_turns * MATH_2PI;
}

void core_init(void)
{
    core.enable = false;
    core.state = INIT;

    // 加载参数

    // 启动foc

    // 启动时间槽
}
void core_reset(void)
{
}

// 核心主循环任务
void core_mainloop_tasks(void)
{
    if (core.enc_enable)
    {
        // TODO:这里暂时只是外部编码器 后续可以改
        encoder_task(&g_enc_ext);
        core.val.theta_enc = encoder_get_angle_abs(&g_enc_ext);
    }

    // 更新电流采集和读取电流值、电压值、温度值
    sense_update(&g_sense, !core.enable);
    sense_get_current(&g_sense, &foc.val.imu, &foc.val.imv, &foc.val.imw);
    core.val.udc = sense_get_vbus(&g_sense);
    core.val.vmax = core.val.udc * MATH_INSQRT3;
    core.val.temp = sense_get_temperature(&g_sense);
}

// 时间槽高频任务
void high_schedule_task0(float ts)
{

    // 数据更新
    if (core.obs_enable && core.enc_enable)
    { // 融合
      // TODO:enc+sm
    }
    else if (core.enc_enable)
    {
        pll_update(&enc_pll, core.val.theta_enc, ts);
        core.val.theta_mech = enc_pll.theta;
        foc.val.theta_elec = (core.val.theta_mech - param.theta_offset) * param.motor_polepairs * (param.positive_dir ? 1 : -1);
        foc.val.theta_elec = normalize_angle_2pi(foc.val.theta_elec);
        core.val.vel = enc_pll.vel;
    }
    else if (core.obs_enable)
    {
        // TODO:HFI+SMO
    }
    else
    { // 开环
        foc.val.theta_elec += foc.tag.vel_elec * ts;
        foc.val.theta_elec = normalize_angle_2pi(foc.val.theta_elec);
    }

    // foc 数据处理
    foc_process(&foc, svpwm.sector);

    if (core.enable)
    {
        // foc更新
        foc_update(&foc);
        // svpwm 调制生成脉冲
        svpwm_update(&svpwm, foc.val.ualpha, foc.val.ubeta);
        // 门极驱动输出
        gate_drv_set_compare(&g_gate, svpwm.ticA, svpwm.ticB, svpwm.ticC);
    }
    else
    { // TODO：高频跟随
    }
}

// TODO:参数校准任务

// 位置累加 更新位置
void medium_schedule_task0(float ts)
{

    pos_accumulate(&core);
}

// 运行轨迹规划器
void medium_schedule_task5(float ts)
{
    if (core.enable)
    {
        // 电流模式不需要轨迹规划
        if (core.run_mode == CURRENT_MODE)
            return;
        traj_Update(&traj, ts);
    }
}
// 分配轨迹规划器输出
void medium_schedule_task6(float ts)
{
    if (core.enable)
    {
        if (core.run_mode == MIT_MODE)
        {
            // 使用内置轨迹规划器 输出轨迹信息
            core.mittag.pos = traj.out.value;
            core.mittag.vel = traj.out.rate_d1;
            core.mittag.tau_ff = param.motor_j * traj.out.rate_d2 + param.motor_b * traj.out.rate_d1;
        }
        else if (core.run_mode == PID_SPEED)
            core.pidtag.vel = traj.out.value;
        else if (core.run_mode == PID_POSITION)
            core.pidtag.pos = traj.out.value;
    }
}

// 跑pid速度环/mit
void medium_schedule_task7(float ts)
{
    if (core.enable)
    {
        if (core.run_mode == PID_SPEED)
        { // 速度环pid控制
            core.pidtag.vel = traj.out.value;
            foc.tag.iq = pi_update(&core.PI_vel, core.pidtag.vel, core.val.vel);
        }
        else if (core.run_mode == MIT_MODE)
        {

            foc.tag.tua = mit_update(&core.mit, core.mittag.tau_ff,
                                     core.mittag.pos, core.val.pos, core.mittag.vel, core.val.vel);
            foc.tag.iq = foc.tag.tua / param.motor_ke;
        }
    }
}
// 弱磁控制
void medium_schedule_task9(float ts)
{
    if (core.enable)
    {

        float vout;
        arm_sqrt_f32((foc.val.ud * foc.val.ud + foc.val.uq * foc.val.uq), &vout);
        float error = core.val.vmax - vout;
        if (error < 0)
            foc.tag.id = pi_update(&core.PI_weakmag, core.val.vmax, vout);
        else
            foc.tag.id = 0.0f;
    }
}

void low_schedule_task0(float ts)
{
}

// pid位置环
void low_schedule_task1(float ts)
{
    if (core.enable)
    {
        if (core.run_mode == PID_POSITION)
        { // 位置环pid控制
            core.pidtag.pos = traj.out.value;
            core.pidtag.vel = pid_update(&core.PID_pos, core.pidtag.pos, core.val.pos);
        }
    }
}
const tSlotTask high_schedule[FREQ_HIGH_LOOP] = {
    {0, high_schedule_task0} // 内环只有一个 foc核心任务
};
const tSlotTask medium_schedule[FREQ_MEDIUM_LOOP];
const tSlotTask low_schedule[FREQ_LOW_LOOP];
