#ifndef MC_TYPES_H
#define MC_TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MC_MODE_OPENLOOP = 0,
    MC_MODE_CURRENT,
    MC_MODE_VELOCITY,
    MC_MODE_POSITION,
    MC_MODE_MIT,
    MC_MODE_IDLE,
    MC_MODE_STARTING
} tMcMode;

static inline bool mc_mode_is_active(tMcMode mode) {
    return (unsigned)mode <= (unsigned)MC_MODE_MIT;
}

typedef struct { float d, q; } tDq;
typedef struct { float alpha, beta; } tAb;

typedef struct {
    float duty[3];
    bool enabled;
} tPwm;

typedef struct {
    float current[3]; /* 服务层提供的 U、V、W 三相电流，单位 A */
    float vbus; /* V，必须为本次有效测量 */
    float angle; /* 原始单圈机械角 rad */
} tMcSample;

typedef struct {
    float theta, theta_e;
    float vel, vel_e;
    float pos; /* 连续机械位置 rad */
} tRotor;

typedef struct {
    float pos, vel;
    float iq, tau_ff;
    float theta_e, vel_e;
    float openloop_duty; /* d 轴占空比，以 Vmax 为基准 */
} tRef;

#endif
