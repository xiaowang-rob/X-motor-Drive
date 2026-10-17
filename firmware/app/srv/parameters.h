#ifndef __PARAMETERS_H
#define __PARAMETERS_H

#include "protocol.h"
#include "field.h"
#include "flash.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    // 电机参数
    // -通用
    uint8_t motor_type;        // 电机类型
    uint8_t motor_polepairs;   // 极对数
    float motor_rated_current; // 额定电流
    float motor_kv;            // KV
    float motor_j;             //
    float motor_b;

    float clbw_coef;
    float dclbw;
    float dclkp;
    float dclki;
    float qclbw;
    float qclkp;
    float qclki;
    float cfalpha;

    // -PMSM
    float motor_rs;
    float motor_ld;
    float motor_lq;
    float motor_psif;
    float motor_ke;
    // -校准
    float tune_current; // 校准电流
    float tune_vel;     // 校准速度
    // 观测器参数
    // -通用
    uint8_t rotor_obs;    // 转子观测器
    uint8_t abs_pos_obs;  // 绝对位置观测器
    float theta_offset;   // 角度偏移
    bool positive_dir;    // 方向反转
    uint8_t theta_source; // 角度源
    uint8_t vel_source;   // 速度源
    // -无感
    float hfi_smo_threshold;
    // -hall enc
    float hall_table[8]; // hall编码器
    // -abi enc
    uint16_t abi_count;

    // -sincos enc
    float sin_amp; // sin 幅值
    float cos_amp; // cos 幅值
    float sin_offset;
    float cos_offset;
    float sincos_fl; // 滤波系数
    // -spi enc
    uint8_t enc_chip;

    // 控制器参数
    // -pid参数
    float vlkp;
    float vlki;
    uint8_t pos_pidmode; // 位置环模式
    float plkp;
    float plki;
    float plkd;
    float plalpha;
    // -mit参数
    float mit_kp;
    float mit_kd;
    float mit_tsta;
    float mit_tmax;
    // -高级
    float fwlkp;
    float fwlki;

    // 轨迹规划器
    uint8_t traj_type;
    float traj_limit_d1;
    float traj_limit_d2;
    float traj_limit_d3;
    float traj_tolerance;

    // 保护
    // -电气保护
    float undervoltage_threshold; // 欠压阈值
    float overvoltage_threshold;  // 过压阈值
    float current_threshold;      // 过流阈值
    float elec_tolerance;         // 电气超限容忍度
    float elec_tolerance_time_ms; // 电气超限容忍时间
    // -运动保护
    float vel_limit;                // 速度限制
    float pos_min_limit;            // 最小位置
    float pos_max_limit;            // 最大位置
    float tau_limit;                // 力矩限制
    float motion_tolerance;         // 运动超限容忍度
    float motion_tolerance_time_ms; // 运动超限容忍时间

    // 通讯
    // -CAN
    uint32_t can_id;
    uint8_t can_mode;

} tParameter;

extern tParameter g_param;

// ---- 读写 / 持久化 ----
// 写入一个参数（value 为该参数的原始字节；id 越界 = "应用并保存" 的协议约定）
void param_set(eParameter para, uint8_t *value);

// 读取一个参数（value 收原始字节，len 出长度；id 越界时 len=0）
void param_get(eParameter para, uint8_t *value, uint8_t *len);

bool param_init(tFlash *flash); // 初始化存储单元、读取 、校验
bool param_save(void);          // 保存进存储单元
bool param_reset(void);         // 恢复默认值

#endif // __PARAMETERS_H
