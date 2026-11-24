#ifndef MC_CONFIG_H
#define MC_CONFIG_H

#include "parameter_list.h"
#include "mc_types.h"

enum { MC_MEDIUM_MIN_DIV = 5, MC_PVC_DIV = 5 };

/* TODO(parameter): 控制周期/分频缺少协议字段，暂由初始化时提供。 */
typedef struct {
    float dt;
    uint16_t medium_div; /* PWM 周期数，至少容纳 5 个中频时隙 */
} tMotorTiming;

tMotorTiming mc_timing_default(float dt);
#ifndef NDEBUG
bool mc_config_valid(const tParameter *param, const tMotorTiming *timing);
#endif

#endif
