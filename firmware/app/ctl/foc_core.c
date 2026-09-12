#include "foc_core.h"
#include "svpwm.h"
#include "math_fast.h"
#include "device.h"
#include "string.h"
#include "smo.h"
#include "tune.h"
#include "loop_control.h"
#include "mit.h"

// HFI→SMO 融合切换速度阈值 [rad/s]

#include "filter.h"
#include "protection_manager.h"
#include "hfi.h"
#include "usr_config.h"
#include "bsp_adc.h"

static float sin_theta_e = 0.0f;
static float cos_theta_e = 0.0f;

tFOC_Mode foc_mode = {0};
tFOC_val foc_val = {0};
tMotor motor = {0};

tFOC_Mode *get_foc_mode_adr()
{
    return &foc_mode;
}
tFOC_val *get_foc_val_adr()
{
    return &foc_val;
}

// 滤波器实例
static tFirstOrderLagFilter _i_u_filter;
static tFirstOrderLagFilter _i_v_filter;
static tFirstOrderLagFilter _i_w_filter;

// 轨迹规划实例
static tTraj_PosOut traj_posout;
static tTraj_VelOut traj_velout;

// PI/PID 控制器实例
static tPI PI_ialpha;
static tPI PI_ibeta;
static tPI PI_iq;
static tPI PI_id;
static tPI PI_weakmag;
static tPI PI_vel;
static tPID PID_pos;

static inline float loop_ualpha_update(float ialpha_ref, float ialpha_fb)
{
    return pi_update(&PI_ialpha, ialpha_ref, ialpha_fb);
}
static inline float loop_ubeta_update(float ibeta_ref, float ibeta_fb)
{
    return pi_update(&PI_ibeta, ibeta_ref, ibeta_fb);
}

// q轴电流环
static inline float loop_uq_update(float iq_ref, float id_fb)
{
    return pi_update(&PI_iq, iq_ref, id_fb);
}

// d轴磁链环
static inline float loop_ud_update(float id_ref, float id_fb)
{
    return pi_update(&PI_id, id_ref, id_fb);
}

// 弱磁控制：电压超限时通过负Id削弱磁链
static inline float loop_id_update(float ud, float uq)
{
    float vout;
    arm_sqrt_f32((ud * ud + uq * uq), &vout);
    float error = foc_val.vmax - vout;
    if (error > 0)
        return 0; // 未超限，无需弱磁
    return pi_update(&PI_weakmag, foc_val.vmax, vout);
}

// 速度环
static inline float loop_iq_update(float vel_ref, float vel_fb)
{
    return pi_update(&PI_vel, vel_ref, vel_fb);
}

// 相对位置环（带指令限幅）
static inline float loop_vel_update(float pos_ref, float pos_fb)
{
    float pos_cmd = CLAMP(pos_ref, foc_val.pos_min, foc_val.pos_max);
    return pid_update(&PID_pos, pos_cmd, pos_fb);
}

// 环路控制器整体初始化
void PID_control_init(tParameter *param)
{
    pi_init(&PI_ialpha, param->kp_D, param->ki_D, foc_val.vmax, g_freqD.t_high);
    pi_init(&PI_ibeta, param->kp_Q, param->ki_Q, foc_val.vmax, g_freqD.t_high);
    pi_init(&PI_iq, param->kp_Q, param->ki_Q, foc_val.vmax, g_freqD.t_high);
    pi_init(&PI_id, param->kp_D, param->ki_D, foc_val.vmax, g_freqD.t_high);
    pi_init(&PI_vel, param->kp_speed, param->ki_speed, param->limit_current, g_freqD.t_med);
    pi_init(&PI_weakmag, param->kp_speed / 2, param->ki_speed / 2, param->limit_current, g_freqD.t_med);
    pid_init(&PID_pos, param->kp_position, param->ki_position, param->kd_position, param->limit_vel, param->alpha_position, g_freqD.t_low);
    foc_val.pos_min = param->limit_position_min;
    foc_val.pos_max = param->limit_position_max;
}

// 重置所有控制器状态
void PID_control_reset(void)
{
    pi_reset(&PI_ialpha);
    pi_reset(&PI_ibeta);
    pi_reset(&PI_iq);
    pi_reset(&PI_id);
    pi_reset(&PI_vel);
    pi_reset(&PI_weakmag);
    pid_reset(&PID_pos);
}

// 启动器初始化
static void _trajectory_init(tParameter *param)
{
    tTraj_Config traj_cfg;
    traj_cfg.limit_d1 = param->traj_limit_d1;
    traj_cfg.limit_d2 = param->traj_limit_d2;
    traj_cfg.limit_d3 = param->traj_limit_d3;
    traj_cfg.tolerance = param->tolerance;
    traj_cfg.type = param->traj_type;
    traj_init(traj_cfg);
}

// 电机参数初始化
static void _motor_init(tParameter *param)
{
    motor.mech_offset = param->theta_offset;
    motor.pole_pairs = param->motor_polepairs;
    motor.elec_pi_offset = param->theta_elec_offset;
    motor.forward_dir = param->forward_dir;
    motor.rs = param->motor_rs;
    motor.ld = param->motor_ld;
    motor.lq = param->motor_lq;
    motor.psi_f = param->motor_psif;
    motor.ke = param->motor_ke;
    motor.j = param->motor_j;
    motor.b = param->motor_b;
}

// 滤波器初始化
static void _filter_init(tParameter *param)
{
    // 从 Hz 计算 alpha = dt / (dt + 1/(2*PI*fc))
    float dt_cur = T_PWM;

    float alpha_cur = dt_cur / (dt_cur + 1.0f / (6.2831853f * CUR_LPF_HZ));

    // 参数中若配置了有效 alpha 值则覆盖
    if (param->cur_filter_alpha > 0.01f && param->cur_filter_alpha < 1.0f)
        alpha_cur = param->cur_filter_alpha;

    filter_first_order_lag_init(&_i_u_filter, alpha_cur, 0);
    filter_first_order_lag_init(&_i_v_filter, alpha_cur, 0);
    filter_first_order_lag_init(&_i_w_filter, alpha_cur, 0);
}

// 模式初始化
static void _mode_init(tParameter *param)
{
    foc_set_sensor_mode(param->sensor_mode);
    foc_mode.run_mode = param->run_mode;
    foc_mode.pvt_mode = param->sw_pvt;
}

// 滤波器复位
void filter_reset()
{
    // 暂时没有需要复位的
}

// FOC核心初始化
void foc_core_init(tParameter *param)
{
    bsp_set_adc_current_offset(param->adc_U_zero_offset, param->adc_V_zero_offset, param->adc_W_zero_offset);
    encoder_init((eEncoderChip)param->encoder_chip);
    _motor_init(param);
    bsp_adc_get_voltage_temp(&foc_val.udc, &foc_val.temp);
    svpwm_init(foc_val.udc);
    foc_val.vmax = foc_val.udc / MATH_SQRT3;
    freq_div_init();
    PID_control_init(param);
    mit_init(param->kp_MIT, param->kd_MIT, motor.j, motor.b, param->tmax_MIT);
    _trajectory_init(param);

    smo_init(&motor);
    _mode_init(param);
    foc_core_reset();
    hfi_init();
    _filter_init(param);
}

// 重置FOC中间变量
static void _foc_val_reset(void)
{
    foc_val.id_ref = 0;
    foc_val.iq_ref = 0;
    foc_val.vel_ref = 0;
    foc_val.pos_ref = 0;
    foc_val.ualpha = 0;
    foc_val.ubeta = 0;
    foc_val.ualpha_hfi = 0;
    foc_val.ubeta_hfi = 0;
    foc_val.ud = 0;
    foc_val.uq = 0;
}

// FOC 复位
void foc_core_reset(void)
{
    hfi_reset_initial_position();
    smo_reset();

    motor_param_tune_reset();

    _foc_val_reset();
    // 刷新电压，确保参数更新后电压环能正确工作
    bsp_adc_get_voltage_temp(&foc_val.udc, &foc_val.temp);

    PID_control_reset();
    svpwm_init(foc_val.udc);

    foc_val.pos_fb = encoder_get_angle_inc();
    foc_val.vel_fb = encoder_get_vel();
    traj_posreset(foc_val.pos_fb);
    traj_velreset(foc_val.vel_fb);
}

// 电流重构 稳定的两相电流重构 + clark 变换
static inline void current_reconstruction(void)
{
    float ui, vi, wi;
    bsp_adc_get_current(&ui, &vi, &wi);
    u8 sector = svpwm_get_sector();
    // 根据扇区确定两相：最短导通相由另两相推导 (Ia+Ib+Ic=0)
    if (sector == 1 || sector == 6)
    { // 最短相=W
        foc_val.iu_im = vi + wi;
        foc_val.iv_im = -vi;
        foc_val.iw_im = -wi;
    }
    else if (sector == 2 || sector == 3)
    { // 最短相=U
        foc_val.iu_im = -ui;
        foc_val.iv_im = ui + wi;
        foc_val.iw_im = -wi;
    }
    else if (sector == 4 || sector == 5)
    { // 最短相=V
        foc_val.iu_im = -ui;
        foc_val.iv_im = -vi;
        foc_val.iw_im = ui + vi;
    }
    else
    { // sector 0/7: 零矢量
        foc_val.iu_im = ui;
        foc_val.iv_im = vi;
        foc_val.iw_im = wi;
    }

    foc_val.iu = filter_first_order_lag(&_i_u_filter, foc_val.iu_im);
    foc_val.iv = filter_first_order_lag(&_i_v_filter, foc_val.iv_im);
    foc_val.iw = filter_first_order_lag(&_i_w_filter, foc_val.iw_im);
    // Clarke 变换
    clarke_transform(foc_val.iu, foc_val.iv, foc_val.iw, &foc_val.ialpha, &foc_val.ibeta);
}

// 更新foc核心变量 20khz
void foc_update_val(void)
{
    // 跟随 pwm 基频运行 更低频的每一段都可能和前面高频的最后段一起执行 所以高频的最后段一般不放任务 按照任务重要程度从第一段开始安排
    freq_div_update();

    // 低频 8 段
    if (g_freqD.low_update[8])
    {
        bsp_adc_get_voltage_temp(&foc_val.udc, &foc_val.temp);
        foc_val.vmax = foc_val.udc / MATH_SQRT3;
    }
    // 中频 0 段
    if (g_freqD.medium_update[0])
    {
        if (foc_mode.sensor_mode == ENCODER_CONTROL || foc_mode.sensor_mode == MERGE_CONTROL)
        { // TODO: 将编码器的pll放入高频 角度也用PLL跟踪
            encoder_pll_update(g_freqD.t_med);
            foc_val.pos_fb = encoder_get_angle_inc();
            foc_val.vel_fb = encoder_get_vel();
        }

        if (foc_mode.sensor_mode == SENSORLESS_CONTROL)
        {
            // TODO: 无感观测器 中频
        }
    }
    // 高频 0 段
    if (g_freqD.high_update[0])
    {
        current_reconstruction();
        if (foc_mode.sensor_mode == ENCODER_CONTROL || foc_mode.sensor_mode == MERGE_CONTROL)
        {
            foc_val.theta_mech = encoder_get_angle_abs();
            foc_val.theta_elec = (foc_val.theta_mech - motor.mech_offset) * motor.pole_pairs * (motor.forward_dir ? 1 : -1) + (motor.elec_pi_offset ? MATH_PI : 0.0f);
            foc_val.theta_elec = normalize_angle_360(foc_val.theta_elec);
        }
        else // 纯无感
        {
            // 融合策略：低速用 HFI，高速用 SMO，中间线性过渡
            // HFI 和 SMO 都在后台运行，这里只做数据融合
            float smo_vel = smo_get_vel() / motor.pole_pairs;
            float smo_theta = smo_get_theta();
            float hfi_theta = hfi_get_theta_elec();
            float vel_abs = FABSF(smo_vel);

            // if (vel_abs < HFI_TO_SMO_VEL)
            // {
            //     // 低速：用 HFI
            //     foc_val.theta_elec = hfi_theta;
            //     foc_val.vel_fb = hfi_get_vel_elec() / motor.pole_pairs;
            // }
            // else if (vel_abs < HFI_TO_SMO_VEL * 1.5f)
            // {
            //     // 过渡区：线性融合
            //     float ratio = (vel_abs - HFI_TO_SMO_VEL) / (HFI_TO_SMO_VEL * 0.5f);
            //     if (ratio > 1.0f)
            //         ratio = 1.0f;
            //     float hfi_vel = hfi_get_vel_elec() / motor.pole_pairs;
            //     foc_val.theta_elec = hfi_theta * (1.0f - ratio) + smo_theta * ratio;
            //     foc_val.vel_fb = hfi_vel * (1.0f - ratio) + smo_vel * ratio;
            // }
            // else
            // {
            //     // 高速：用 SMO
            //     foc_val.theta_elec = smo_theta;
            //     foc_val.vel_fb = smo_vel;
            // }
        }
        arm_sin_cos_rad_f32(foc_val.theta_elec, &sin_theta_e, &cos_theta_e);
        park_transform(foc_val.ialpha, foc_val.ibeta, sin_theta_e, cos_theta_e, &foc_val.id_fb, &foc_val.iq_fb);
    }
}

// 使能后执行：按模式运行对应控制环
void foc_main_loop_task(void)
{
    if (foc_mode.sensor_mode == SENSORLESS_CONTROL || foc_mode.sensor_mode == MERGE_CONTROL)
    {
        if (g_freqD.high_update[0])
        { // HFI 无感观测器
            hfi_step(foc_val.ialpha, foc_val.ibeta, &foc_val.ualpha_hfi, &foc_val.ubeta_hfi);

            if (!hfi_get_status()) // 初始极性辨识
            {
                // todo:使能之后 直接跑电压环
                hfi_detect_initial_position(foc_val.id_fb, &foc_val.ualpha, &foc_val.ubeta);
                goto voltage_loop;
            }
        }
    }

    switch (foc_mode.run_mode)
    {

    case PID_POSITION:
        // 低频 0 段 - 位置轨迹计算
        if (g_freqD.low_update[0])
        {
            traj_posout = traj_PosUpdate(g_freqD.t_low);
            foc_val.pos_ref = traj_posout.value;
        }
        // 低频 1 段 - 位置环计算
        if (g_freqD.low_update[1])
            foc_val.vel_ref = loop_vel_update(foc_val.pos_ref, foc_val.pos_fb);
        // 中频 1 段 - 速度环计算
        if (g_freqD.medium_update[1])
        {
            foc_val.iq_ref = loop_iq_update(foc_val.vel_ref, foc_val.vel_fb);
            foc_val.id_ref = loop_id_update(foc_val.ud, foc_val.uq);
        }
        goto current_loop;

    case PID_SPEED:
        // 中频 1 段 - 速度规划计算
        if (g_freqD.medium_update[1])
        {
            traj_velout = traj_VelUpdate(g_freqD.t_med);
            foc_val.vel_ref = traj_velout.value;
        }
        // 中频 2 段 - 速度环计算
        if (g_freqD.medium_update[2])
        {
            foc_val.iq_ref = loop_iq_update(foc_val.vel_ref, foc_val.vel_fb);
            foc_val.id_ref = loop_id_update(foc_val.ud, foc_val.uq);
        }
        goto current_loop;

    case MIT_POSITION: // TODO: 要做单位换算
                       // 中频 1 段 - 位置规划计算
        if (g_freqD.medium_update[1])
        {
            traj_posout = traj_PosUpdate(g_freqD.t_med);
            foc_val.pos_ref = traj_posout.value;
        }
        // 中频 2 段 - MIT力矩计算
        if (g_freqD.medium_update[2])
        {
            foc_val.tau_ref = mit_pos_update(traj_posout.accel, foc_val.pos_ref, foc_val.pos_fb, foc_val.vel_fb);
            foc_val.iq_ref = foc_val.tau_ref / motor.ke; // V·s/rad (或 V/(rad/s)) 整定出的ke要做单位换算
            foc_val.id_ref = 0;
        }
        goto current_loop;

    case MIT_SPEED:
        // 中频 1 段 - 速度规划计算
        if (g_freqD.medium_update[1])
        {
            traj_velout = traj_VelUpdate(g_freqD.t_med);
            foc_val.vel_ref = traj_velout.value;
        }
        // 中频 2 段 - MIT力矩计算
        if (g_freqD.medium_update[2])
        {
            foc_val.tau_ref = mit_vel_update(traj_velout.accel, foc_val.vel_ref, foc_val.vel_fb);
            foc_val.iq_ref = foc_val.tau_ref / motor.ke; // V·s/rad (或 V/(rad/s)) 整定出的ke要做单位换算
            foc_val.id_ref = 0;
        }
        goto current_loop;

    case MIT_TRAJ:
        // 中频 1 段 - MIT轨迹模式计算
        if (g_freqD.medium_update[1])
        {
            foc_val.tau_ref = mit_track_update(traj_posout.accel, foc_val.tau_ff_ref, foc_val.pos_ref, foc_val.pos_fb, foc_val.vel_ref, foc_val.vel_fb);
            foc_val.iq_ref = foc_val.tau_ref / motor.ke; // V·s/rad (或 V/(rad/s)) 整定出的ke要做单位换算
            foc_val.id_ref = 0;
        }
        goto current_loop;

        // 电流环
    case CURRENT_MODE:
    current_loop:
        // 高频 0 段 - 电流环计算
        if (g_freqD.high_update[0])
        {
            foc_val.uq = loop_uq_update(foc_val.iq_ref, foc_val.iq_fb);
            foc_val.ud = loop_ud_update(foc_val.id_ref, foc_val.id_fb);
            inv_park_transform(foc_val.ud, foc_val.uq, sin_theta_e, cos_theta_e, &foc_val.ualpha, &foc_val.ubeta);
        }
        break;
        // 开环电流
    case OPEN_CUR:
        // 高频 0 段 - 开环计算
        if (g_freqD.high_update[0])
        {
            foc_val.ol_theta_elec += foc_val.ol_vel_elec * g_freqD.t_high;
            arm_sin_cos_rad_f32(foc_val.ol_theta_elec, &sin_theta_e, &cos_theta_e);
            inv_park_transform(foc_val.ol_cur_ref, 0, sin_theta_e, cos_theta_e, &foc_val.ialpha_ref, &foc_val.ibeta_ref);
            foc_val.ualpha = loop_ualpha_update(foc_val.ialpha_ref, foc_val.ialpha);
            foc_val.ubeta = loop_ubeta_update(foc_val.ibeta_ref, foc_val.ibeta);
        }
        break;
    default: // 开环电压
        break;
    }
voltage_loop: // 电压环 跟随基频运行
    svpwm_run(foc_val.ualpha + foc_val.ualpha_hfi, foc_val.ubeta + foc_val.ubeta_hfi);
    // svpwm_sample_point_calibration();  // 暂不使用采样点调整
}

// 设置各环指令值
void foc_set_target(float *value)
{
    switch (foc_mode.run_mode)
    {
    case CURRENT_MODE:
        foc_val.iq_ref = value[0];
        foc_val.id_ref = value[1];
        break;
    case PID_SPEED:
        traj_set_veltarget(value[0]);
        break;
    case PID_POSITION:
        if (foc_mode.pvt_mode == PVT_PV)
        { // TODO: 实现 PV闭环控制 不能与 轨迹规划同时使用
            float max_vel = FABSF(value[1]);

            if (max_vel <= 0.01f)
                max_vel = 0.01f;
            if (max_vel > g_Param.limit_vel)
                max_vel = g_Param.limit_vel;
            PID_pos.output_limit = max_vel;
        }
        else if (foc_mode.pvt_mode == PVT_PT)
        { // TODO: 实现 PT闭环控制
        }
        else
            traj_set_postarget(value[0]);
        break;
    case MIT_POSITION:
        traj_set_postarget(value[0]);
        break;
    case MIT_SPEED:
        traj_set_veltarget(value[0]);
        break;
    case MIT_TRAJ:
        // TODO:  到时候四个can帧发送完成后同时写入 目标位置 目标速度 目标加速度 前馈扭矩
        // 不能用内置轨迹规划器
        foc_val.mit_target.acc = value[0];
        foc_val.mit_target.vel = value[1];
        foc_val.mit_target.pos = value[2];
        foc_val.mit_target.tau_ff = value[3];
        break;
    case OPEN_CUR:
        foc_val.ol_cur_ref = value[0];
        foc_val.ol_vel_elec = value[1];
        break;
    case OPEN_VOL:
        foc_val.ualpha = value[0];
        foc_val.ubeta = value[1];
        break;
    default:
        break;
    }
}

void foc_set_ol_vel_cur(float vel_elec, float cur_ref)
{
    foc_val.ol_cur_ref = cur_ref;
    foc_val.ol_vel_elec = vel_elec;
}

// 设置 αβ 电压
// void foc_set_ualpha_beta(float Ualpha, float Ubeta)
// {
//     foc_val.ualpha = Ualpha;
//     foc_val.ubeta = Ubeta;
// }
// 设置 dq 电流
// void foc_set_id_iq(float id, float iq)
// {
//     foc_val.id_ref = id;
//     foc_val.iq_ref = iq;
// }
// 设置开环电流控制的目标电角度和电流
// theta_elec: 电角度 [rad]，固定值不会被积分更新
// cur_ref:    q 轴电流参考 [A]（开环模式 id=0, iq=cur_ref）
void foc_set_ol_theta_cur(float theta_elec, float cur_ref)
{
    foc_val.ol_theta_elec = theta_elec;
    foc_val.ol_vel_elec = 0.0f; // 禁止角度积分
    foc_val.ol_cur_ref = cur_ref;
}
void foc_set_cur_loop_param(float Kp_d, float Ki_d, float Kp_q, float Ki_q)
{
    pi_init(&PI_ialpha, Kp_d, Ki_d, foc_val.vmax, g_freqD.t_high);
    pi_init(&PI_ibeta, Kp_d, Ki_d, foc_val.vmax, g_freqD.t_high);
    pi_init(&PI_iq, Kp_q, Ki_q, foc_val.vmax, g_freqD.t_high);
    pi_init(&PI_id, Kp_d, Ki_d, foc_val.vmax, g_freqD.t_high);
}
// 设置编码器零点偏移
void foc_set_theta_offset(float thetaoffset)
{
    motor.mech_offset = thetaoffset;
}

// 切换传感模式
void foc_set_sensor_mode(eSensorMode mode)
{
    foc_core_reset();
    foc_mode.sensor_mode = mode;
}
// 切换控制模式
void foc_set_run_mode(eRunMode mode)
{
    _foc_val_reset();
    foc_mode.run_mode = mode;
}

// 强制刹车
bool foc_shutdown(void)
{
    if (fabsf(foc_val.vel_fb) < 0.01f)
        return true;
    foc_set_run_mode(PID_SPEED);
    float vel_shutdown = -foc_val.vel_fb * 0.5f;
    foc_set_target(&vel_shutdown);
    return false;
}
// 设置位置零点
void foc_set_zero_pos()
{
    encoder_set_angle_zero();
}
// 设置限位位置
void foc_set_limit_pos()
{
    if (foc_val.pos_fb > 0)
        g_Param.limit_position_max = foc_val.pos_fb;
    else
        g_Param.limit_position_min = foc_val.pos_fb;
    foc_core_init(&g_Param);
    pro_set_limit_position(g_Param.limit_position_min, g_Param.limit_position_max);
}
