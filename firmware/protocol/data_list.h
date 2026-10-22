/* 此文件由 protocol/gen_protocol.py 自动生成，请勿手动修改 */

#ifndef __DATA_LIST_H
#define __DATA_LIST_H

#include "fieldvaluetype.h"

// 观测变量 ID 枚举
typedef enum
{
    DATA_CURRENT_U = 0x01,     // U相电流
    DATA_CURRENT_V = 0x02,     // V相电流
    DATA_CURRENT_W = 0x03,     // W相电流
    DATA_VOLTAGE_Q = 0x04,     // Q轴电压
    DATA_VOLTAGE_D = 0x05,     // D轴电压
    DATA_CURRENT_ALPHA = 0x06, // α轴电流
    DATA_CURRENT_BETA = 0x07,  // β轴电流
    DATA_CURRENT_Q = 0x08,     // Q轴电流
    DATA_CURRENT_D = 0x09,     // D轴电流
    DATA_CURRENT_Q_REF = 0x0A, // Q轴电流参考
    DATA_CURRENT_D_REF = 0x0B, // D轴电流参考
    DATA_VELOCITY = 0x0C,      // 速度
    DATA_VELOCITY_REF = 0x0D,  // 速度参考
    DATA_THETA_ELEC = 0x0E,    // 电角度
    DATA_THETA_MECH = 0x0F,    // 机械角度
    DATA_POSITION = 0x10,      // 位置
    DATA_POSITION_REF = 0x11,  // 位置参考
    DATA_MINUTES = 0x12,       // 分钟
    DATA_SECONDS = 0x13,       // 秒
    DATA_STATE = 0x14,         // 状态
    DATA_FAULT = 0x15,         // 故障
    DATA_WARNING = 0x16,       // 警告
    DATA_VBUS = 0x17,          // 母线电压
    DATA_DRIVER_TEMP = 0x18,   // 驱动温度
    DATA_MOTOR_TEMP = 0x19     // 电机温度
} eObsData;

// ---------- 观测选项 ----------
// 核心状态
typedef enum
{
    CORE_INIT = 0,         // INIT
    CORE_IDLE = 1,         // IDLE
    CORE_TUNING = 2,       // TUNING
    CORE_FAULT = 3,        // FAULT
    CORE_WARNING = 4,      // WARNING
    CORE_OPENLOOP = 5,     // 开环模式
    CORE_CURRENT_MODE = 6, // 电流模式
    CORE_PID_SPEED = 7,    // PID速度模式
    CORE_PID_POSITION = 8, // PID位置模式
    CORE_MIT_MODE = 9      // MIT模式
} eCoreState;

// 错误
typedef enum
{
    FAULT_NONE = 0,               // NONE
    FAULT_OBS_INIT = 1,           // 观测器初始化失败
    FAULT_BUSCOM_INIT = 2,        // 总线通信初始化失败
    FAULT_RS_TUNE = 3,            // 电阻整定失败
    FAULT_RS_IMBALANCE = 4,       // 相电阻不平衡
    FAULT_LS_TUNE = 5,            // 电感整定失败
    FAULT_PSIF_TUNE = 6,          // 磁链整定失败
    FAULT_OBS_TUNE = 7,           // 观测器校准失败
    FAULT_POLE_PAIR_MISMATCH = 8, // 极对数不匹配
    FAULT_MOTOR_LOCK = 9,         // 电机堵转
    FAULT_MECH_PARAM_FAIL = 10,   // 机械参数校准失败
    FAULT_OVERVOLTAGE = 11,       // 过电压
    FAULT_UNDERVOLTAGE = 12,      // 低电压
    FAULT_OVERCURRENT = 13        // 过电流
} eFault;

// 警告
typedef enum
{
    WARNING_NONE = 0,           // NONE
    WARNING_OVERTEMP = 1,       // 过温
    WARNING_OVERSPEED = 2,      // 超速
    WARNING_POSITION_LIMIT = 3, // 位置超限
    WARNING_BUSCOM_RUN = 4,     // 总线通讯异常
    WARNING_OBS_RUN = 5         // 观测器运行异常
} eWarning;

typedef struct
{
    float *current_u;     // U相电流
    float *current_v;     // V相电流
    float *current_w;     // W相电流
    float *voltage_q;     // Q轴电压
    float *voltage_d;     // D轴电压
    float *current_alpha; // α轴电流
    float *current_beta;  // β轴电流
    float *current_q;     // Q轴电流
    float *current_d;     // D轴电流
    float *current_q_ref; // Q轴电流参考
    float *current_d_ref; // D轴电流参考
    float *velocity;      // 速度
    float *velocity_ref;  // 速度参考
    float *theta_elec;    // 电角度
    float *theta_mech;    // 机械角度
    float *position;      // 位置
    float *position_ref;  // 位置参考
    float *minutes;       // 分钟
    float *seconds;       // 秒
    uint8_t *state;       // 状态
    uint8_t *fault;       // 故障
    uint8_t *warning;     // 警告
    float *vbus;          // 母线电压
    float *driver_temp;   // 驱动温度
    float *motor_temp;    // 电机温度
} tObsData;

#define DATA_NUM 0x1A

#define DATA_FIELDS(X)                            \
    X(DATA_CURRENT_U, current_u, FLD_F32)         \
    X(DATA_CURRENT_V, current_v, FLD_F32)         \
    X(DATA_CURRENT_W, current_w, FLD_F32)         \
    X(DATA_VOLTAGE_Q, voltage_q, FLD_F32)         \
    X(DATA_VOLTAGE_D, voltage_d, FLD_F32)         \
    X(DATA_CURRENT_ALPHA, current_alpha, FLD_F32) \
    X(DATA_CURRENT_BETA, current_beta, FLD_F32)   \
    X(DATA_CURRENT_Q, current_q, FLD_F32)         \
    X(DATA_CURRENT_D, current_d, FLD_F32)         \
    X(DATA_CURRENT_Q_REF, current_q_ref, FLD_F32) \
    X(DATA_CURRENT_D_REF, current_d_ref, FLD_F32) \
    X(DATA_VELOCITY, velocity, FLD_F32)           \
    X(DATA_VELOCITY_REF, velocity_ref, FLD_F32)   \
    X(DATA_THETA_ELEC, theta_elec, FLD_F32)       \
    X(DATA_THETA_MECH, theta_mech, FLD_F32)       \
    X(DATA_POSITION, position, FLD_F32)           \
    X(DATA_POSITION_REF, position_ref, FLD_F32)   \
    X(DATA_MINUTES, minutes, FLD_F32)             \
    X(DATA_SECONDS, seconds, FLD_F32)             \
    X(DATA_STATE, state, FLD_U8)                  \
    X(DATA_FAULT, fault, FLD_U8)                  \
    X(DATA_WARNING, warning, FLD_U8)              \
    X(DATA_VBUS, vbus, FLD_F32)                   \
    X(DATA_DRIVER_TEMP, driver_temp, FLD_F32)     \
    X(DATA_MOTOR_TEMP, motor_temp, FLD_F32)

#endif
