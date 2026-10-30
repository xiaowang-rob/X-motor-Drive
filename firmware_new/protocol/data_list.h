/* 此文件由 protocol/gen_protocol.py 自动生成，请勿手动修改 */
#ifndef PROTO_DATA_LIST_H
#define PROTO_DATA_LIST_H

#include "fieldvaluetype.h"

typedef enum
{
    DATA_CURRENT_U = 0x01, /* U相电流 */
    DATA_CURRENT_V = 0x02, /* V相电流 */
    DATA_CURRENT_W = 0x03, /* W相电流 */
    DATA_VOLTAGE_Q = 0x04, /* Q轴电压 */
    DATA_VOLTAGE_D = 0x05, /* D轴电压 */
    DATA_CURRENT_ALPHA = 0x06, /* α轴电流 */
    DATA_CURRENT_BETA = 0x07, /* β轴电流 */
    DATA_CURRENT_Q = 0x08, /* Q轴电流 */
    DATA_CURRENT_D = 0x09, /* D轴电流 */
    DATA_CURRENT_Q_REF = 0x0A, /* Q轴电流参考 */
    DATA_CURRENT_D_REF = 0x0B, /* D轴电流参考 */
    DATA_VELOCITY = 0x0C, /* 速度 */
    DATA_VELOCITY_REF = 0x0D, /* 速度参考 */
    DATA_THETA_ELEC = 0x0E, /* 电角度 */
    DATA_THETA_MECH = 0x0F, /* 机械角度 */
    DATA_POSITION = 0x10, /* 位置 */
    DATA_POSITION_REF = 0x11, /* 位置参考 */
    DATA_STATE = 0x12, /* 状态 */
    DATA_FAULT = 0x13, /* 故障 */
    DATA_WARNING = 0x14, /* 警告 */
    DATA_VBUS = 0x15, /* 母线电压 */
    DATA_DRIVER_TEMP = 0x16, /* 驱动温度 */
    DATA_MOTOR_TEMP = 0x17, /* 电机温度 */
} eObsData;

typedef enum
{
    CORE_INIT = 0, /* INIT */
    CORE_IDLE = 1, /* IDLE */
    CORE_TUNING = 2, /* TUNING */
    CORE_FAULT = 3, /* FAULT */
    CORE_WARNING = 4, /* WARNING */
    CORE_OPENLOOP = 5, /* 开环模式 */
    CORE_CURRENT_MODE = 6, /* 电流模式 */
    CORE_PID_SPEED = 7, /* PID速度模式 */
    CORE_PID_POSITION = 8, /* PID位置模式 */
    CORE_MIT_MODE = 9, /* MIT模式 */
} eCoreState;

typedef enum
{
    FAULT_NONE = 0, /* NONE */
    FAULT_OBS_INIT = 1, /* 观测器初始化失败 */
    FAULT_BUSCOM_INIT = 2, /* 总线通信初始化失败 */
    FAULT_RS_TUNE = 3, /* 电阻整定失败 */
    FAULT_RS_IMBALANCE = 4, /* 相电阻不平衡 */
    FAULT_LS_TUNE = 5, /* 电感整定失败 */
    FAULT_PSIF_TUNE = 6, /* 磁链整定失败 */
    FAULT_OBS_TUNE = 7, /* 观测器校准失败 */
    FAULT_POLE_PAIR_MISMATCH = 8, /* 极对数不匹配 */
    FAULT_MOTOR_LOCK = 9, /* 电机堵转 */
    FAULT_MECH_PARAM_FAIL = 10, /* 机械参数校准失败 */
    FAULT_OVERVOLTAGE = 11, /* 过电压 */
    FAULT_UNDERVOLTAGE = 12, /* 低电压 */
    FAULT_OVERCURRENT = 13, /* 过电流 */
} eFault;

typedef enum
{
    WARNING_NONE = 0, /* NONE */
    WARNING_OVERTEMP = 1, /* 过温 */
    WARNING_OVERSPEED = 2, /* 超速 */
    WARNING_POSITION_LIMIT = 3, /* 位置超限 */
    WARNING_BUSCOM_RUN = 4, /* 总线通讯异常 */
    WARNING_OBS_RUN = 5, /* 观测器运行异常 */
    WARNING_INVALID_INPUT = 6, /* 非法输入 */
} eWarning;

typedef struct
{
    float *current_u;
    float *current_v;
    float *current_w;
    float *voltage_q;
    float *voltage_d;
    float *current_alpha;
    float *current_beta;
    float *current_q;
    float *current_d;
    float *current_q_ref;
    float *current_d_ref;
    float *velocity;
    float *velocity_ref;
    float *theta_elec;
    float *theta_mech;
    float *position;
    float *position_ref;
    uint8_t *state;
    uint8_t *fault;
    uint8_t *warning;
    float *vbus;
    float *driver_temp;
    float *motor_temp;
} tObsData;

#define DATA_NUM 0x18

#define DATA_FIELDS(X) \
    X(DATA_CURRENT_U, current_u, FLD_F32) \
    X(DATA_CURRENT_V, current_v, FLD_F32) \
    X(DATA_CURRENT_W, current_w, FLD_F32) \
    X(DATA_VOLTAGE_Q, voltage_q, FLD_F32) \
    X(DATA_VOLTAGE_D, voltage_d, FLD_F32) \
    X(DATA_CURRENT_ALPHA, current_alpha, FLD_F32) \
    X(DATA_CURRENT_BETA, current_beta, FLD_F32) \
    X(DATA_CURRENT_Q, current_q, FLD_F32) \
    X(DATA_CURRENT_D, current_d, FLD_F32) \
    X(DATA_CURRENT_Q_REF, current_q_ref, FLD_F32) \
    X(DATA_CURRENT_D_REF, current_d_ref, FLD_F32) \
    X(DATA_VELOCITY, velocity, FLD_F32) \
    X(DATA_VELOCITY_REF, velocity_ref, FLD_F32) \
    X(DATA_THETA_ELEC, theta_elec, FLD_F32) \
    X(DATA_THETA_MECH, theta_mech, FLD_F32) \
    X(DATA_POSITION, position, FLD_F32) \
    X(DATA_POSITION_REF, position_ref, FLD_F32) \
    X(DATA_STATE, state, FLD_U8) \
    X(DATA_FAULT, fault, FLD_U8) \
    X(DATA_WARNING, warning, FLD_U8) \
    X(DATA_VBUS, vbus, FLD_F32) \
    X(DATA_DRIVER_TEMP, driver_temp, FLD_F32) \
    X(DATA_MOTOR_TEMP, motor_temp, FLD_F32)
#endif
