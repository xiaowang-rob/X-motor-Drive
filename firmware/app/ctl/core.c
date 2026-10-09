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

static tFirstOrderLagFilter cf_u, cf_v, cf_w; // 电流滤波器
static tPLL obs_pll;
static tTraj traj;
static tPosAcc pos_acc;

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
    // 初始化slot槽服务
    slot_con_init(F_PWM);

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

    core.fb.udc = sense_get_vbus(&g_sense);
    core.fb.vmax = core.fb.udc * MATH_INSQRT3;
    // 初始化svpwm
    svpwm_init(&svpwm, TIC_PWM, core.fb.udc, T_PWM, T_SAMPLE, T_NOISE, T_DIED);

    // foc初始化
    foc_init(&foc, &g_param, g_slotcon.t_high, g_slotcon.t_med, g_slotcon.t_low, core.fb.vmax);

    // 控制外环 pid mit 初始化
    core_loop_init(g_slotcon.t_med, g_slotcon.t_low);

    // 初始化观测器
    core_obs_init();

    // 初始化 内置轨迹规划
    core_traj_init();

    // 电流滤波初始化
    filter_first_order_lag_init(&cf_u, g_param.cfalpha, 0);
    filter_first_order_lag_init(&cf_v, g_param.cfalpha, 0);
    filter_first_order_lag_init(&cf_w, g_param.cfalpha, 0);

    // 将solt槽任务注册到pwm下溢中断 以 启动solt槽任务
    pwm_register_callback(NULL, slot_con_update);

    return core_init_ok;
}
void core_reset(void)
{

    filter_first_order_lag_reset(&cf_u, 0);
    filter_first_order_lag_reset(&cf_v, 0);
    filter_first_order_lag_reset(&cf_w, 0);

    memset(&core.ref, 0, sizeof(core.ref));
    memset(&core.fb, 0, sizeof(core.fb));
}

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
    switch (core.obs)
    {
    case NONE_OBS:
        break;
    case ENCODER_SPI:
        // TODO:这里暂时只是外部编码器 后续可以改
        encoder_task(&g_enc_ext);
        core.fb.theta_enc = encoder_get_angle_abs(&g_enc_ext);
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
    if (core.enc_enable)
    { // 这样写是优先编码器 如果无感观测器和编码器都使能了 也优先编码器
        // TODO:20khz pll跟随1khz角度变化 需要1khz的角度突变处理
        pll_update(&obs_pll, core.fb.theta_enc, ts);
        core.fb.theta_mech = obs_pll.theta;
        core.fb.theta_elec = (core.fb.theta_mech - g_param.theta_offset) * g_param.motor_polepairs * (g_param.positive_dir ? 1 : -1);
        core.fb.theta_elec = normalize_angle_2pi(core.fb.theta_elec);
        core.fb.vel = obs_pll.vel;
    }
    else if (core.obs_enable)
    {
        // TODO:HFI+SMO
    }
    else
    { // 开环
        core.fb.theta_elec += core.ref.vel_elec * ts;
        core.fb.theta_elec = normalize_angle_2pi(core.fb.theta_elec);
    }

    // foc 数据处理
    current_process(svpwm.sector, core.fb.ims, core.fb.is);
    foc_process(&foc, svpwm.sector);

    if (core.enable)
    {
        // foc更新
        if (!core.ov_enable)
            foc_update(&foc);

        inv_park_transform(core.fb.ud, core.fb.uq, core.fb.sin_e, core.fb.cos_e,
                           &core.fb.ualpha, &core.fb.ubeta);

        // svpwm 调制生成脉冲
        svpwm_update(&svpwm, core.fb.ualpha, core.fb.ubeta);
        // 门极驱动输出
        gate_drv_set_compare(&g_gate, svpwm.ticA, svpwm.ticB, svpwm.ticC);
    }
}

// TODO:参数校准任务

// 位置累加 更新位置
void pos_acc_task(float ts)
{
    pos_accumulate(&pos_acc, core.fb.theta_mech);
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
            core.ref.iq = pi_update(&core.PI_vel, core.pidtag.vel, core.fb.vel);
        }
        else if (core.ctrl_mode == MIT_MODE)
        {

            core.ref.tau = mit_update(&core.mit, core.mittag.tau_ff,
                                      core.mittag.pos, core.fb.pos, core.mittag.vel, core.fb.vel);
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
            core.ref.id = pi_update(&core.PI_weakmag, core.fb.vmax, vout);
        else
            core.ref.id = 0.0f;
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
            core.pidtag.vel = pid_update(&core.PID_pos, core.pidtag.pos, core.fb.pos);
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
