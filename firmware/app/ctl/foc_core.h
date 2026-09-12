#ifndef __FOC_CORE_H
#define __FOC_CORE_H

#include "bsp_base.h"
#include "parameter_manager.h"
#include "trajectory.h"

#include "protocol.h"

typedef struct
{
    float cur_filter_alpha;   // 电流滤波系数
    float speed_filter_alpha; // 速度滤波系数
    eSensorMode sensor_mode;
    eRunMode run_mode;
    u8 pvt_mode;
    eTrajType trajectory_mode;
} tFOC_Mode;

// mit 参数
typedef struct
{
    float acc;    // 加速度 (rad/s^2)
    float vel;    // 速度 (rad/s)
    float pos;    // 位置 (rad)
    float tau_ff; // 转矩前馈
} tMIT_target;
// 转矩前馈为0,则使用加速度 转矩前馈不为0,则使用转矩前馈 加速度作废

typedef struct
{
    float udc, vmax, temp;
    float iu_im, iv_im, iw_im;
    float iu, iv, iw;
    float ialpha, ibeta;
    float ialpha_ref, ibeta_ref;
    float ol_cur_ref;
    float ol_vel_elec;
    float ol_theta_elec;
    float theta_elec;
    float theta_mech;
    float iq_ref, id_ref;
    float iq_fb, id_fb;
    float ud, uq;
    float ualpha, ubeta;
    float ualpha_hfi, ubeta_hfi;
    float vel_ref;
    float vel_fb;
    float pos_max;
    float pos_min;
    float pos_ref;
    float pos_fb;
    float tau_ref;
    float tau_ff_ref;
    tMIT_target mit_target;
} tFOC_val;

typedef struct
{
    float mech_offset; // 机械偏移角度
    float rs;          // 定子电阻
    float ld;          // 定子电感
    float lq;
    float psi_f;         // 永磁体磁链
    float ke;            // 反电动势常数
    float j;             // 转动惯量
    float b;             // 摩擦系数
    u8 pole_pairs;       // 极对数
    bool forward_dir;    // 正转方向
    bool elec_pi_offset; // 电角度180°偏差
} tMotor;

tFOC_Mode *get_foc_mode_adr();
tFOC_val *get_foc_val_adr();

void foc_core_init(tParameter *param);
void foc_param_update(tParameter *param);
void foc_core_reset();

void foc_update_val();
void foc_main_loop_task();
bool foc_shutdown();

// 辅助整定 函数
void filter_reset();
void foc_set_ol_theta_cur(float theta_elec, float cur_ref);
void foc_set_ol_vel_cur(float vel_elec, float cur_ref);
void foc_set_cur_loop_param(float Kp_d, float Ki_d, float Kp_q, float Ki_q);
void foc_set_ualpha_beta(float Ubeta);
void foc_set_id_iq(float id, float iq);
void foc_set_theta_offset(float thetaoffset);

// 主要函数

void foc_set_target(float *value);
void foc_set_sensor_mode(eSensorMode mode);
void foc_set_run_mode(eRunMode mode);
void foc_set_zero_pos();
void foc_set_limit_pos();

#endif // __FOC_CORE_H