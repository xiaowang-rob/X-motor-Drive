#ifndef __PARAMETERS_H
#define __PARAMETERS_H

#include "protocol.h"
#include "field.h"
#include "flash.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    // 配置参数
    uint8_t ienc_mode;
    uint8_t ienc_chip;
    uint8_t eenc_mode;
    uint8_t eenc_chip;

    bool enc_active;
    bool obs_active;
    uint8_t ctrl_mode;
    uint8_t traj_type;

    // 电机参数
    uint8_t motor_polepairs;
    bool positive_dir;

    float theta_offset;
    float motor_kv;
    float motor_rs;
    float motor_ld;
    float motor_lq;
    float motor_psif;
    float motor_ke;
    float motor_j;
    float motor_b;
    // 控制参数
    uint32_t can_id;
    uint8_t can_mode;
    float clbw_coef;
    float qclkp;
    float qclki;
    float dclkp;
    float dclki;
    float cfalpha;
    float vlkp;
    float vlki;
    float wlkp;
    float wlki;
    float plkp;
    float plki;
    float plkd;
    float plalpha;

    float mit_kp;
    float mit_kd;
    float mit_tsta;
    float mit_tmax;

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
    float traj_tolerance;

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
