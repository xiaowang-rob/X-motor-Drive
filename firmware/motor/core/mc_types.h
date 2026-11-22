#ifndef MC_TYPES_H
#define MC_TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MC_OK = 0,
    MC_INVALID_ARGUMENT,
    MC_INVALID_CONFIG,
    MC_BUSY,
    MC_NOT_READY,
    MC_UNSUPPORTED,
    MC_FAULT_LATCHED
} tMcResult;

typedef enum {
    MC_MODE_OPENLOOP = 0,
    MC_MODE_CURRENT,
    MC_MODE_TORQUE,
    MC_MODE_VELOCITY,
    MC_MODE_POSITION,
    MC_MODE_MIT
} tMcMode;

typedef enum { MC_IDLE = 0, MC_STARTING, MC_RUNNING, MC_FAULT } tMcRunState;

typedef enum {
    MC_FAULT_NONE = 0,
    MC_FAULT_SAMPLE = 1u << 0,
    MC_FAULT_SENSOR = 1u << 1,
    MC_FAULT_BUS = 1u << 2,
    MC_FAULT_EXTERNAL = 1u << 4
} tMcFault;

enum { MC_PHASE_U = 1u, MC_PHASE_V = 2u, MC_PHASE_W = 4u,
       MC_PHASE_ALL = 7u };

typedef struct { float d, q; } tDq;
typedef struct { float alpha, beta; } tAb;

/* 编号标识本次采样对应的已施加 PWM，而非即将计算的 PWM。 */
typedef struct {
    float duty[3];
    uint8_t sector;                 /* 电压矢量扇区 1..6；零矢量为 0 */
    uint32_t sequence;
    bool enabled;
} tPwm;

typedef struct {
    float current[3];               /* 校准后的带符号相电流 A */
    uint8_t phase_mask;     /* 两相或三相；无效相允许为 NaN */
    float vbus;             /* V，必须为本次有效测量 */
    float angle;             /* 原始单圈机械角 rad */
    bool rotor_valid;
    uint32_t pwm_sequence;
} tSample;

typedef struct {
    float theta, theta_e;
    float vel, vel_e;
    float pos;               /* 连续机械位置 rad */
    bool valid;
} tRotor;

typedef struct {
    float pos, vel;
    float iq, tau_ff, kp, kd;
    float theta_e, vel_e;
    tDq voltage;                 /* 开环电压 V */
} tRef;

#endif
