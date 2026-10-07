#include "core.h"

#include "bsp_cfg.h"
#include "bsp_irq.h"
#include "bsp_math.h"

#include "encoder.h"
#include "bus_com.h"
#include "sense.h"

#include "foc.h"
#include "svpwm.h"
#include "pll.h"
#include "trajectory.h"
#include "pacc.h"

#include "protection.h"
#include "slot_con.h"
#include "status_feedback.h"

tCore core;
tFOC foc;
tSvpwm svpwm;

static tPLL obs_pll;
static tTraj traj;
static tPosAcc pos_acc;

// 控制外环初始化 PI速度环 pid位置环 mit控制环 t_vl:速度环周期 t_pl:位置环周期
static inline void core_loop_init(float t_vl, float t_pl)
{
    pi_init(&core.PI_vel, g_param.vlkp, g_param.vlki, g_param.limit_current, t_vl);
    pi_init(&core.PI_weakmag, g_param.vlkp / 2, g_param.vlki / 2, g_param.limit_current, t_vl);
    pid_init(&core.PID_pos, g_param.plkp, g_param.plki, g_param.plkd, g_param.limit_vel, g_param.plalpha, t_pl);
    mit_init(&core.mit, g_param.mit_kp, g_param.mit_kd, g_param.mit_tsta, g_param.mit_tmax);
}
// 控制外环复位 PI速度环 pid位置环复位 mit/pid指令值归零
static inline void core_loop_reset(void)
{
    pi_reset(&core.PI_vel);
    pi_reset(&core.PI_weakmag);
    pid_reset(&core.PID_pos);

    memset(&core.pidtag, 0, sizeof(tPIDtarget));
    memset(&core.mittag, 0, sizeof(tMITtarget));
}

#define ENCODER_PLL_KP 80.0f
#define ENCODER_PLL_KI 2000.0f
#define ENCODER_PLL_INTEG_LIMIT 0.1745f // 积分限值  ±10°
#define ENCODER_VEL_PHYS_LIMIT 1046.0f  // rad/s 物理上限（≈10k rpm）

#define HFISMO_PLL_KP 80.0f
#define HFISMO_PLL_KI 2000.0f
#define HFISMO_PLL_INTEG_LIMIT 0.1745f // 积分限值  ±10°
#define HFISMO_VEL_PHYS_LIMIT 2000.0f  // rad/s 物理上限（≈10k rpm）

// 观测器初始化
static inline void core_obs_init(void)
{
    switch (core.obs)
    {
    case NONE_OBS:
        core.enc_enable = false;
        core.obs_enable = false;
        break;
    case ENCODER_SPI:
    case ENCODER_ABZ:
    case ENCODER_SINCOS:
        core.enc_enable = true;
        core.obs_enable = false;

        pll_init(&obs_pll, ENCODER_PLL_KP, ENCODER_PLL_KI,
                 ENCODER_PLL_INTEG_LIMIT, ENCODER_VEL_PHYS_LIMIT);
        break;
    case HFI_SMO:
        core.enc_enable = false;
        core.obs_enable = true;

        pll_init(&obs_pll, HFISMO_PLL_KP, HFISMO_PLL_KI,
                 HFISMO_PLL_INTEG_LIMIT, HFISMO_VEL_PHYS_LIMIT);
        break;
    default:
        break;
    }
}
// 内置轨迹规划器初始化
static inline void core_traj_init(void)
{
    tTraj_Config cfg = {
        .limit_d1 = g_param.traj_limit_d1,
        .limit_d2 = g_param.traj_limit_d2,
        .limit_d3 = g_param.traj_limit_d3,
        .tolerance = g_param.traj_tolerance,
        .type = g_param.traj_type,
    };
    traj_init(&traj, cfg);
}
bool core_init(void)
{
    core.enable = false;
    core.state = INIT;
    bool core_init_ok = false;

    // 初始化core配置
    core.obs_enable = g_param.obs_active;
    core.enc_enable = g_param.enc_active;
    core.ctrl_mode = g_param.ctrl_mode;

    // 初始化保护服务
    pro_manager_init(&g_param);
    // 配置编码器设备驱动
    core_init_ok = bsp_enc_init((eEncoderMode)g_param.ienc_mode,
                                (eEncoderMode)g_param.eenc_mode, (eEncoderChip)g_param.eenc_chip);
    // 启动主总线通信
    core_init_ok = bus_start(&g_can, g_param.can_id);

    core.val.udc = sense_get_vbus(&g_sense);
    core.val.vmax = core.val.udc * MATH_INSQRT3;
    // 初始化svpwm
    svpwm_init(&svpwm, TIC_PWM, core.val.udc, T_PWM, T_SAMPLE, T_NOISE, T_DIED);

    // foc初始化
    foc_init(&foc, &g_param, T_CON, core.val.vmax);

    // 控制外环 pid mit 初始化
    core_loop_init(g_slotcon.t_med, g_slotcon.t_low);

    // 初始化观测器
    core_obs_init();

    // 初始化 内置轨迹规划
    core_traj_init();

    slot_con_init(F_PWM);
    // 将solt槽任务注册到pwm下溢中断 以 启动solt槽任务
    pwm_register_callback(NULL, slot_con_update);

    return core_init_ok;
}
void core_reset(void)
{
}

// 核心主循环任务
void core_mainloop_tasks(void)
{
    switch (core.obs)
    {
    case NONE_OBS:
        break;
    case ENCODER_SPI:
        // TODO:这里暂时只是外部编码器 后续可以改
        encoder_task(&g_enc_ext);
        core.val.theta_enc = encoder_get_angle_abs(&g_enc_ext);
        break;
    case ENCODER_ABZ:
        break;
    case ENCODER_SINCOS:
        break;
    case HFI_SMO:
        break;
    default:
        break;
    }

    // 更新电流采集和读取电流值、电压值、温度值
    sense_update(&g_sense, !core.enable);
    sense_get_current(&g_sense, &foc.val.imu, &foc.val.imv, &foc.val.imw);
    core.val.udc = sense_get_vbus(&g_sense);
    core.val.vmax = core.val.udc * MATH_INSQRT3;
    core.val.temp = sense_get_temperature(&g_sense);

    status_feedback_main_loop(core.state);
}

// 时间槽高频任务
void obs_foc_task(float ts)
{
    // 观测器运行
    if (core.enc_enable)
    { // 这样写是优先编码器 如果无感观测器和编码器都使能了 也优先编码器
        // TODO:20khz pll跟随1khz角度变化 需要1khz的角度突变处理
        pll_update(&obs_pll, core.val.theta_enc, ts);
        core.val.theta_mech = obs_pll.theta;
        foc.val.theta_elec = (core.val.theta_mech - g_param.theta_offset) * g_param.motor_polepairs * (g_param.positive_dir ? 1 : -1);
        foc.val.theta_elec = normalize_angle_2pi(foc.val.theta_elec);
        core.val.vel = obs_pll.vel;
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
        foc.val.ud += foc.tag.ud_hfi;

        inv_park_transform(foc.val.ud, foc.val.uq, foc.val.sin_e, foc.val.cos_e,
                           &foc.val.ualpha, &foc.val.ubeta);

        // svpwm 调制生成脉冲
        svpwm_update(&svpwm, foc.val.ualpha, foc.val.ubeta);
        // 门极驱动输出
        gate_drv_set_compare(&g_gate, svpwm.ticA, svpwm.ticB, svpwm.ticC);
    }
}

// TODO:参数校准任务

// 位置累加 更新位置
void pos_acc_task(float ts)
{
    pos_accumulate(&pos_acc, core.val.theta_mech);
}

// 运行轨迹规划器
void traj_task(float ts)
{
    if (core.enable)
    {
        // 电流模式不需要轨迹规划
        if (core.ctrl_mode == CURRENT_MODE)
            return;
        traj_Update(&traj, ts);
        if (core.ctrl_mode == MIT_MODE && traj.cfg.type != TRAJ_DISABLE)
        {
            // 使用内置轨迹规划器 输出轨迹信息
            core.mittag.pos = traj.out.value;
            core.mittag.vel = traj.out.rate_d1;
            core.mittag.tau_ff = g_param.motor_j * traj.out.rate_d2 + g_param.motor_b * traj.out.rate_d1;
        }
        else if (core.ctrl_mode == PID_SPEED)
            core.pidtag.vel = traj.out.value;
        else if (core.ctrl_mode == PID_POSITION)
            core.pidtag.pos = traj.out.value;
    }
}

// 跑pid速度环/mit
void vl_pid_mit_task(float ts)
{
    if (core.enable)
    {
        if (core.ctrl_mode == PID_SPEED)
        { // 速度环pid控制
            core.pidtag.vel = traj.out.value;
            foc.tag.iq = pi_update(&core.PI_vel, core.pidtag.vel, core.val.vel);
        }
        else if (core.ctrl_mode == MIT_MODE)
        {

            foc.tag.tua = mit_update(&core.mit, core.mittag.tau_ff,
                                     core.mittag.pos, core.val.pos, core.mittag.vel, core.val.vel);
            foc.tag.iq = foc.tag.tua / g_param.motor_ke;
        }
    }
}
// 弱磁控制
void weak_mag_task(float ts)
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

// pid位置环
void pl_pid_task(float ts)
{
    if (core.enable)
    {
        if (core.ctrl_mode == PID_POSITION)
        { // 位置环pid控制
            core.pidtag.pos = traj.out.value;
            core.pidtag.vel = pid_update(&core.PID_pos, core.pidtag.pos, core.val.pos);
        }
    }
}
const tSlotTask high_schedule[FREQ_HIGH_LOOP] = {
    {0, obs_foc_task} // 内环只有一个 foc核心任务
};
const tSlotTask medium_schedule[FREQ_MEDIUM_LOOP] = {
    {0, pos_acc_task},
    {4, traj_task},
    {5, vl_pid_mit_task},
    {6, weak_mag_task},
};
const tSlotTask low_schedule[FREQ_LOW_LOOP] = {
    {1, pl_pid_task},
};
