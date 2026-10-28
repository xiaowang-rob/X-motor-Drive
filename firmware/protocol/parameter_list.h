/* 此文件由 protocol/gen_protocol.py 自动生成，请勿手动修改 */
#ifndef PROTO_PARAM_LIST_H
#define PROTO_PARAM_LIST_H

#include "fieldvaluetype.h"

typedef enum
{
    PARAM_MOTOR_TYPE = 0x01, /* 电机类型 */
    PARAM_MOTOR_POLEPAIRS = 0x02, /* 极对数 */
    PARAM_MOTOR_KV = 0x03, /* 电机KV值 */
    PARAM_MOTOR_RATED_CURRENT = 0x04, /* 额定电流 */
    PARAM_MOTOR_RS = 0x05, /* 定子电阻(Rs) */
    PARAM_MOTOR_LD = 0x06, /* d轴电感(Ld) */
    PARAM_MOTOR_LQ = 0x07, /* q轴电感(Lq) */
    PARAM_MOTOR_PSIF = 0x08, /* 永磁体磁链(λ) */
    PARAM_MOTOR_KE = 0x09, /* 反电动势常数(Ke) */
    PARAM_MOTOR_J = 0x0A, /* 转动惯量(J) */
    PARAM_MOTOR_B = 0x0B, /* 粘滞摩擦系数(B) */
    PARAM_CLBW_COEF = 0x0C, /* 电流环带宽系数 */
    PARAM_DCLBW = 0x0D, /* d轴电流环带宽 */
    PARAM_DCLKP = 0x0E, /* d轴电流环Kp */
    PARAM_DCLKI = 0x0F, /* d轴电流环Ki */
    PARAM_QCLBW = 0x10, /* q轴电流环带宽 */
    PARAM_QCLKP = 0x11, /* q轴电流环Kp */
    PARAM_QCLKI = 0x12, /* q轴电流环Ki */
    PARAM_CFALPHA = 0x13, /* 电流滤波系数 */
    PARAM_TUNE_CURRENT = 0x14, /* 校准电流 */
    PARAM_TUNE_VEL = 0x15, /* 校准速度 */
    PARAM_ROTOR_OBS = 0x16, /* 转子观测器 */
    PARAM_ABS_POS_OBS = 0x17, /* 绝对位置观测器 */
    PARAM_THETA_OFFSET = 0x18, /* 角度偏移 */
    PARAM_POSITIVE_DIR = 0x19, /* 方向反转 */
    PARAM_THETA_SOURCE = 0x1A, /* 角度源 */
    PARAM_VEL_SOURCE = 0x1B, /* 速度源 */
    PARAM_HFI_SMO_THRESHOLD = 0x1C, /* HFI/SMO切换阈值 */
    PARAM_HALL_TABLE_0 = 0x1D, /* HALL编码器表0 */
    PARAM_HALL_TABLE_1 = 0x1E, /* HALL编码器表1 */
    PARAM_HALL_TABLE_2 = 0x1F, /* HALL编码器表2 */
    PARAM_HALL_TABLE_3 = 0x20, /* HALL编码器表3 */
    PARAM_HALL_TABLE_4 = 0x21, /* HALL编码器表4 */
    PARAM_HALL_TABLE_5 = 0x22, /* HALL编码器表5 */
    PARAM_HALL_TABLE_6 = 0x23, /* HALL编码器表6 */
    PARAM_HALL_TABLE_7 = 0x24, /* HALL编码器表7 */
    PARAM_ABI_COUNT = 0x25, /* ABI编码器线数 */
    PARAM_SIN_AMP = 0x26, /* SIN幅值 */
    PARAM_COS_AMP = 0x27, /* COS幅值 */
    PARAM_SIN_OFFSET = 0x28, /* SIN偏移 */
    PARAM_COS_OFFSET = 0x29, /* COS偏移 */
    PARAM_SINCOS_FL = 0x2A, /* SINCOS滤波系数 */
    PARAM_ENC_CHIP = 0x2B, /* 编码器芯片 */
    PARAM_VLKP = 0x2C, /* 速度环Kp */
    PARAM_VLKI = 0x2D, /* 速度环Ki */
    PARAM_VELOCITY_LIMIT = 0x2E, /* 速度限制 */
    PARAM_POS_PIDMODE = 0x2F, /* 位置环模式 */
    PARAM_PLKP = 0x30, /* 位置环Kp */
    PARAM_PLKI = 0x31, /* 位置环Ki */
    PARAM_PLKD = 0x32, /* 位置环Kd */
    PARAM_PLALPHA = 0x33, /* 位置环滤波系数 */
    PARAM_POS_LIMIT_MODE = 0x34, /* 位置限制模式 */
    PARAM_POS_MIN_LIMIT = 0x35, /* 最小位置 */
    PARAM_POS_MAX_LIMIT = 0x36, /* 最大位置 */
    PARAM_MIT_KP = 0x37, /* MIT 刚度(Kp) */
    PARAM_MIT_KD = 0x38, /* MIT 阻尼(Kd) */
    PARAM_MIT_KPMAX = 0x39, /* MIT 最大刚度 */
    PARAM_MIT_KDMAX = 0x3A, /* MIT 最大阻尼 */
    PARAM_MIT_TMAX = 0x3B, /* MIT最大扭矩 */
    PARAM_FWLKP = 0x3C, /* 弱磁Kp */
    PARAM_FWLKI = 0x3D, /* 弱磁Ki */
    PARAM_TRAJ_MODE = 0x3E, /* 轨迹规划器模式 */
    PARAM_TRAJ_LIMIT_D1 = 0x3F, /* 轨迹一阶限幅 */
    PARAM_TRAJ_LIMIT_D2 = 0x40, /* 轨迹二阶限幅 */
    PARAM_TRAJ_LIMIT_D3 = 0x41, /* 轨迹三阶限幅 */
    PARAM_TRAJ_TOLERANCE = 0x42, /* 轨迹容差 */
    PARAM_UNDERVOLTAGE_THRESHOLD = 0x43, /* 欠压阈值 */
    PARAM_OVERVOLTAGE_THRESHOLD = 0x44, /* 过压阈值 */
    PARAM_OVERTEMP_THRESHOLD = 0x45, /* 过温阈值 */
    PARAM_PRO_TOLERANCE = 0x46, /* 保护超限容忍度 */
    PARAM_PRO_TOLERANCE_TIME_MS = 0x47, /* 保护超限容忍时间 */
    PARAM_CAN_RATE = 0x48, /* CAN速率 */
    PARAM_CAN_ID = 0x49, /* CAN ID */
    PARAM_CAN_MODE = 0x4A, /* CAN模式 */
} eParameter;

typedef enum
{
    MOTOR_PMSM = 0, /* PMSM-永磁同步电机 */
    MOTOR_BLDC = 1, /* BLDC-无刷直流电机 */
} eMotorList;

typedef enum
{
    OBS_NONE = 0, /* 无观测 */
    OBS_INT_ENC = 1, /* 内置编码器 */
    OBS_ENC_HALL = 2, /* 霍尔编码器 */
    OBS_ENC_SPI = 3, /* SPI编码器 */
    OBS_ENC_ABI = 4, /* ABI编码器 */
    OBS_ENC_SINCOS = 5, /* 正余弦编码器 */
    OBS_HFI_SMO = 6, /* HFI+SMO观测器 */
} eObsList;

typedef enum
{
    THETA_PLL = 0, /* PLL */
    THETA_RAW = 1, /* 原始角度 */
} eThetaSource;

typedef enum
{
    VEL_PLL = 0, /* PLL */
    VEL_FDM = 1, /* 差分速度 */
} eVelSource;

typedef enum
{
    ENC_MT6816 = 0, /* MT6816 */
    ENC_MT6835 = 1, /* MT6835 */
    ENC_AS5047 = 2, /* AS5047 */
} eSpiEncList;

typedef enum
{
    POS_PID_PVC = 0, /* PVC三环级联(抗干扰) */
    POS_PID_PC = 1, /* PC双环(高响应) */
} ePosPidmode;

typedef enum
{
    POS_LIMIT_DISABLE = 0, /* 禁用 */
    POS_LIMIT_MIN = 1, /* 最小位置限制 */
    POS_LIMIT_MAX = 2, /* 最大位置限制 */
    POS_LIMIT_BOTH = 3, /* 双端位置限制 */
} ePosLimitMode;

typedef enum
{
    TRAJ_DISABLE = 0, /* 禁用 */
    TRAJ_TRAPEZOID = 1, /* 梯形轨迹(斜坡缓冲) */
    TRAJ_S_CURVE = 2, /* 曲线轨迹(双重缓冲) */
} eTrajMode;

typedef enum
{
    CAN_RATE_1000K = 0, /* 1Mbps */
    CAN_RATE_500K = 1, /* 500Kbps */
} eCanRate;

typedef enum
{
    CAN_SILENCE = 0, /* 静默模式 */
    CAN_ANSWER = 1, /* 响应模式 */
} eCanMode;

typedef struct
{
    uint8_t motor_type;
    uint8_t motor_polepairs;
    float motor_kv;
    float motor_rated_current;
    float motor_rs;
    float motor_ld;
    float motor_lq;
    float motor_psif;
    float motor_ke;
    float motor_j;
    float motor_b;
    float clbw_coef;
    float dclbw;
    float dclkp;
    float dclki;
    float qclbw;
    float qclkp;
    float qclki;
    float cfalpha;
    float tune_current;
    float tune_vel;
    uint8_t rotor_obs;
    uint8_t abs_pos_obs;
    float theta_offset;
    bool positive_dir;
    uint8_t theta_source;
    uint8_t vel_source;
    float hfi_smo_threshold;
    uint8_t hall_table_0;
    uint8_t hall_table_1;
    uint8_t hall_table_2;
    uint8_t hall_table_3;
    uint8_t hall_table_4;
    uint8_t hall_table_5;
    uint8_t hall_table_6;
    uint8_t hall_table_7;
    uint16_t abi_count;
    float sin_amp;
    float cos_amp;
    float sin_offset;
    float cos_offset;
    float sincos_fl;
    uint8_t enc_chip;
    float vlkp;
    float vlki;
    float velocity_limit;
    uint8_t pos_pidmode;
    float plkp;
    float plki;
    float plkd;
    float plalpha;
    uint8_t pos_limit_mode;
    float pos_min_limit;
    float pos_max_limit;
    float mit_kp;
    float mit_kd;
    float mit_kpmax;
    float mit_kdmax;
    float mit_tmax;
    float fwlkp;
    float fwlki;
    uint8_t traj_mode;
    float traj_limit_d1;
    float traj_limit_d2;
    float traj_limit_d3;
    float traj_tolerance;
    float undervoltage_threshold;
    float overvoltage_threshold;
    float overtemp_threshold;
    float pro_tolerance;
    float pro_tolerance_time_ms;
    uint8_t can_rate;
    uint32_t can_id;
    uint8_t can_mode;
} tParameter;

#define PARAM_NUM 0x4B

#define PARAM_FIELDS(X) \
    X(PARAM_MOTOR_TYPE, motor_type, FLD_U8, 0u) \
    X(PARAM_MOTOR_POLEPAIRS, motor_polepairs, FLD_U8, 7u) \
    X(PARAM_MOTOR_KV, motor_kv, FLD_F32, 550.0f) \
    X(PARAM_MOTOR_RATED_CURRENT, motor_rated_current, FLD_F32, 3.0f) \
    X(PARAM_MOTOR_RS, motor_rs, FLD_F32, 0.5f) \
    X(PARAM_MOTOR_LD, motor_ld, FLD_F32, 4e-06f) \
    X(PARAM_MOTOR_LQ, motor_lq, FLD_F32, 4e-06f) \
    X(PARAM_MOTOR_PSIF, motor_psif, FLD_F32, 0.0f) \
    X(PARAM_MOTOR_KE, motor_ke, FLD_F32, 0.0f) \
    X(PARAM_MOTOR_J, motor_j, FLD_F32, 0.0f) \
    X(PARAM_MOTOR_B, motor_b, FLD_F32, 0.0f) \
    X(PARAM_CLBW_COEF, clbw_coef, FLD_F32, 0.5f) \
    X(PARAM_DCLBW, dclbw, FLD_F32, 0.0f) \
    X(PARAM_DCLKP, dclkp, FLD_F32, 0.0f) \
    X(PARAM_DCLKI, dclki, FLD_F32, 0.0f) \
    X(PARAM_QCLBW, qclbw, FLD_F32, 0.0f) \
    X(PARAM_QCLKP, qclkp, FLD_F32, 0.0f) \
    X(PARAM_QCLKI, qclki, FLD_F32, 0.0f) \
    X(PARAM_CFALPHA, cfalpha, FLD_F32, 0.5f) \
    X(PARAM_TUNE_CURRENT, tune_current, FLD_F32, 10.0f) \
    X(PARAM_TUNE_VEL, tune_vel, FLD_F32, 20.0f) \
    X(PARAM_ROTOR_OBS, rotor_obs, FLD_U8, 0u) \
    X(PARAM_ABS_POS_OBS, abs_pos_obs, FLD_U8, 0u) \
    X(PARAM_THETA_OFFSET, theta_offset, FLD_F32, 0.0f) \
    X(PARAM_POSITIVE_DIR, positive_dir, FLD_BOOL, false) \
    X(PARAM_THETA_SOURCE, theta_source, FLD_U8, 0u) \
    X(PARAM_VEL_SOURCE, vel_source, FLD_U8, 0u) \
    X(PARAM_HFI_SMO_THRESHOLD, hfi_smo_threshold, FLD_F32, 0.0f) \
    X(PARAM_HALL_TABLE_0, hall_table_0, FLD_U8, 255u) \
    X(PARAM_HALL_TABLE_1, hall_table_1, FLD_U8, 255u) \
    X(PARAM_HALL_TABLE_2, hall_table_2, FLD_U8, 255u) \
    X(PARAM_HALL_TABLE_3, hall_table_3, FLD_U8, 255u) \
    X(PARAM_HALL_TABLE_4, hall_table_4, FLD_U8, 255u) \
    X(PARAM_HALL_TABLE_5, hall_table_5, FLD_U8, 255u) \
    X(PARAM_HALL_TABLE_6, hall_table_6, FLD_U8, 255u) \
    X(PARAM_HALL_TABLE_7, hall_table_7, FLD_U8, 255u) \
    X(PARAM_ABI_COUNT, abi_count, FLD_U16, 8192u) \
    X(PARAM_SIN_AMP, sin_amp, FLD_F32, 1.0f) \
    X(PARAM_COS_AMP, cos_amp, FLD_F32, 1.0f) \
    X(PARAM_SIN_OFFSET, sin_offset, FLD_F32, 1.65f) \
    X(PARAM_COS_OFFSET, cos_offset, FLD_F32, 1.65f) \
    X(PARAM_SINCOS_FL, sincos_fl, FLD_F32, 0.5f) \
    X(PARAM_ENC_CHIP, enc_chip, FLD_U8, 0u) \
    X(PARAM_VLKP, vlkp, FLD_F32, 0.0f) \
    X(PARAM_VLKI, vlki, FLD_F32, 0.0f) \
    X(PARAM_VELOCITY_LIMIT, velocity_limit, FLD_F32, 500.0f) \
    X(PARAM_POS_PIDMODE, pos_pidmode, FLD_U8, 0u) \
    X(PARAM_PLKP, plkp, FLD_F32, 0.0f) \
    X(PARAM_PLKI, plki, FLD_F32, 0.0f) \
    X(PARAM_PLKD, plkd, FLD_F32, 0.0f) \
    X(PARAM_PLALPHA, plalpha, FLD_F32, 0.5f) \
    X(PARAM_POS_LIMIT_MODE, pos_limit_mode, FLD_U8, 0u) \
    X(PARAM_POS_MIN_LIMIT, pos_min_limit, FLD_F32, -15000.0f) \
    X(PARAM_POS_MAX_LIMIT, pos_max_limit, FLD_F32, 15000.0f) \
    X(PARAM_MIT_KP, mit_kp, FLD_F32, 0.0f) \
    X(PARAM_MIT_KD, mit_kd, FLD_F32, 0.0f) \
    X(PARAM_MIT_KPMAX, mit_kpmax, FLD_F32, 0.0f) \
    X(PARAM_MIT_KDMAX, mit_kdmax, FLD_F32, 0.0f) \
    X(PARAM_MIT_TMAX, mit_tmax, FLD_F32, 0.0f) \
    X(PARAM_FWLKP, fwlkp, FLD_F32, 0.0f) \
    X(PARAM_FWLKI, fwlki, FLD_F32, 0.0f) \
    X(PARAM_TRAJ_MODE, traj_mode, FLD_U8, 0u) \
    X(PARAM_TRAJ_LIMIT_D1, traj_limit_d1, FLD_F32, 0.0f) \
    X(PARAM_TRAJ_LIMIT_D2, traj_limit_d2, FLD_F32, 0.0f) \
    X(PARAM_TRAJ_LIMIT_D3, traj_limit_d3, FLD_F32, 0.0f) \
    X(PARAM_TRAJ_TOLERANCE, traj_tolerance, FLD_F32, 0.0f) \
    X(PARAM_UNDERVOLTAGE_THRESHOLD, undervoltage_threshold, FLD_F32, 20.0f) \
    X(PARAM_OVERVOLTAGE_THRESHOLD, overvoltage_threshold, FLD_F32, 26.0f) \
    X(PARAM_OVERTEMP_THRESHOLD, overtemp_threshold, FLD_F32, 80.0f) \
    X(PARAM_PRO_TOLERANCE, pro_tolerance, FLD_F32, 0.1f) \
    X(PARAM_PRO_TOLERANCE_TIME_MS, pro_tolerance_time_ms, FLD_F32, 1.0f) \
    X(PARAM_CAN_RATE, can_rate, FLD_U8, 0u) \
    X(PARAM_CAN_ID, can_id, FLD_U32, 1u) \
    X(PARAM_CAN_MODE, can_mode, FLD_U8, 0u)
#endif
