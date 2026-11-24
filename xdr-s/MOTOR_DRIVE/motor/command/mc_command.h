#ifndef MC_COMMAND_H
#define MC_COMMAND_H

#include "core/mc_types.h"
#include "parameter_list.h"

typedef struct {
    tMcMode mode;
    union {
        bool openloop_speed_control;
        bool torque_control;
        bool fw_enable;
        struct { float kp, kd; } mit;
    } config;
} tCmdCfg;

typedef struct {
    tMcMode mode;
    union {
        struct {
            float value; /* 电角度 rad 或电角速度 rad/s，由配置决定 */
            float duty;  /* Vmax 基准，范围 [0, 1] */
        } openloop;
        float current; /* A 或 N·m，由配置决定 */
        float vel;
        struct { float pos; } position;
        struct { float pos, vel, tau_ff; } mit;
    } value;
} tCmdValue;

tCmdCfg cmd_default(tMcMode mode, const tParameter *param);
#ifndef NDEBUG
bool cmd_config_valid(const tCmdCfg *cmd, const tParameter *param);
#endif

#endif
