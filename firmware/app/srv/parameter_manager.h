#ifndef __PARAMETER_MANAGER_H
#define __PARAMETER_MANAGER_H

#include "bsp_flash.h"
#include "protocol.h"

typedef struct
{
    // u8类型参数
    u8 none_flag;
    u8 encoder_chip;
    u8 sw_canqueue;
    u8 sw_vague_pid;
    u8 sw_pvt;
    u8 traj_type;
    u8 sensor_mode;
    u8 run_mode;

    u8 motor_polepairs;
    bool theta_elec_offset;
    bool forward_dir;

    // u32类型参数
    u32 can_id;

    // float类型参数

    float theta_offset;
    float motor_kv;
    float motor_rs;
    float motor_ld;
    float motor_lq;
    float motor_psif;
    float motor_ke;
    float motor_j;
    float motor_b;

    float kp_speed;
    float ki_speed;
    float kp_position;
    float ki_position;
    float kd_position;
    float alpha_position;
    float kp_MIT;
    float kd_MIT;
    float tmax_MIT;
    float tune_current;
    float limit_current;
    float limit_vel;
    float limit_position_min;
    float limit_position_max;
    float tolerance_time;
    float tolerance_limit;

    float traj_limit_d1;
    float traj_limit_d2;
    float traj_limit_d3;
    float tolerance;

    // 不需要上位机改写的参数可以放在这里，避免误改
    float kp_Q;
    float ki_Q;
    float kp_D;
    float ki_D;
    float kp_weakmag;
    float ki_weakmag;

    float cur_filter_alpha;   // 电流滤波系数
    float speed_filter_alpha; // 速度滤波系数
    float adc_U_zero_offset;  // ADC零点补偿
    float adc_V_zero_offset;  // ADC零点补偿
    float adc_W_zero_offset;  // ADC零点补偿
} tParameter;

extern tParameter g_Param;

void param_set(eParameter para, u8 *value);
void param_get(eParameter para, u8 *value, u8 *len);
bool param_save();  // 一键保存
bool param_erase(); // 一键擦除
bool param_init();

// 参数描述符表 — 用 memcpy+offsetof 替代 switch-case
typedef struct
{
    u16 offset;
    u8 size;
} tParamEntry;

extern const tParamEntry g_param_table[];

#endif // __PARAMETER_MANAGER_H