#ifndef MC_CONFIG_H
#define MC_CONFIG_H

#include "parameter_list.h"
#include "mc_types.h"

/* TODO(parameter): 控制周期/分频缺少协议字段，暂由初始化时提供。 */
typedef struct {
    float dt;
    uint16_t cmd_div;
    uint16_t vel_div;
    uint16_t pos_div;
    uint16_t slow_div;
} tMotorTiming;

tMotorTiming mc_timing_default(float dt);
tMcResult mc_config_validate(const tParameter *param, const tMotorTiming *timing);

#endif
