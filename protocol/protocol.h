/* ===== 此文件由 gen_protocol.py 自动生成，请勿手动修改，相关配置在 protocol.json 中 ===== */
#ifndef __PROTOCOL_H
#define __PROTOCOL_H

typedef enum
{
    ENCODER_CHIP = 0,        /* 编码器芯片 */
    SENSOR_MODE = 1,         /* 感应模式 */
    RUN_MODE = 2,            /* 运行模式 */
    CAN_MODE = 3,            /* CAN模式 */
    VAGUE_PID_MODE = 4,      /* 模糊PID */
    PVT_MODE = 5,            /* PVT模式 */
    TRAJ_TYPE = 6,           /* 轨迹规划器类型 */
    MOTOR_POLEPAIRS = 7,     /* 电机极对数 */
    CAN_ID = 8,              /* CAN ID */
    THETA_OFFSET = 9,        /* 角度补偿 */
    MOTOR_KV = 10,           /* KV */
    MOTOR_RS = 11,           /* 相电阻 */
    MOTOR_Ld = 12,           /* Ld */
    MOTOR_Lq = 13,           /* Lq */
    MOTOR_PSIF = 14,         /* 磁链 */
    MOTOR_KE = 15,           /* 反电动势常数 */
    MOTOR_J = 16,            /* 转动惯量 */
    MOTOR_B = 17,            /* 摩擦系数 */
    KP_SPEED = 18,           /* 速度环比例 */
    KI_SPEED = 19,           /* 速度环积分 */
    KP_POSITION = 20,        /* 位置环比例 */
    KI_POSITION = 21,        /* 位置环积分 */
    KD_POSITION = 22,        /* 位置环微分 */
    ALPHA_POSITION = 23,     /* 位置环滤波系数 */
    MIT_KP = 24,             /* MIT刚度 */
    MIT_KD = 25,             /* MIT阻尼 */
    MIT_TMAX = 26,           /* MIT最大扭矩 */
    TUNE_CURRENT = 27,       /* 校准电流 */
    LIMIT_CURRENT = 28,      /* 电流限幅 */
    LIMIT_VELOCITY = 29,     /* 速度限幅 */
    LIMIT_POSITION_MIN = 30, /* 位置限幅最小值 */
    LIMIT_POSITION_MAX = 31, /* 位置限幅最大值 */
    TOLERANCE_TIME = 32,     /* 容忍时间 */
    TOLERANCE_LIMIT = 33,    /* 超限容忍度 */
    TRAJ_LIMIT_D1 = 34,      /* 一阶限幅 */
    TRAJ_LIMIT_D2 = 35,      /* 二阶限幅 */
    TRAJ_LIMIT_D3 = 36,      /* 三阶限幅 */
    TRAJ_TOLERANCE = 37,     /* 轨迹规划容差 */
    PARAM_NUM = 38
} eParameter;

typedef enum
{
    CURRENT_U = 0,      /* I_u */
    CURRENT_V = 1,      /* I_v */
    CURRENT_W = 2,      /* I_w */
    VOLTAGE_Q = 3,      /* V_q */
    VOLTAGE_D = 4,      /* V_d */
    CURRENT_ALPHA = 5,  /* I_α */
    CURRENT_BETA = 6,   /* I_β */
    CURRENT_Q = 7,      /* I_q */
    CURRENT_D = 8,      /* I_d */
    CURRENT_Q_REF = 9,  /* Iq_ref */
    CURRENT_D_REF = 10, /* Id_ref */
    SPEED = 11,         /* vel */
    SPEED_REF = 12,     /* vel_ref */
    THETA_ELEC = 13,    /* theta_e */
    THETA_MECH = 14,    /* theta_m */
    POSITION = 15,      /* pos */
    POSITION_REF = 16,  /* pos_ref */
    DATA_NUM = 17
} eData_stream;

typedef enum
{
    ENCODER_CONTROL = 0,    /* 编码反馈 */
    SENSORLESS_CONTROL = 1, /* 无感观测 */
    MERGE_CONTROL = 2,      /* 混合模式 */
} eSensorMode;

typedef enum
{
    MT6816 = 0,     /* MT6816 */
    MT6835 = 1,     /* MT6835 */
    AS5047 = 2,     /* AS5047 */
    CHIP_COUNT = 3, /* 芯片数量 */
} eEncoderChip;

typedef enum
{
    OPEN_VOL = 0,     /* 开环电压 */
    OPEN_CUR = 1,     /* 开环电流 */
    CURRENT_MODE = 2, /* 电流模式 */
    PID_SPEED = 3,    /* PID速度模式 */
    PID_POSITION = 4, /* PID位置模式 */
    MIT_SPEED = 5,    /* MIT速度模式 */
    MIT_POSITION = 6, /* MIT位置模式 */
    MIT_TRAJ = 7,     /* MIT轨迹模式 */
} eRunMode;

typedef enum
{
    CURRENT_TARGET = 0,  /* 拖动电流/A */
    SPEED_TARGET = 1,    /* 目标速度/rpm */
    POSITION_TARGET = 2, /* 目标位置/deg */
    NONE = 3,            /* 无 */
} eTargetType;

typedef enum
{
    CAN_REALTIME = 0,    /* 实时处理 */
    CAN_QUEUE = 1,       /* 队列处理 */
    CAN_REALTIME_FB = 2, /* 实时反馈 */
    CAN_QUEUE_FB = 3,    /* 队列反馈 */
} eCanMode;

typedef enum
{
    VAGUE_PID_DISABLE = 0, /* 禁用 */
    VAGUE_PID_ENABLE = 1,  /* 启动 */
} eVaguePIDMode;

typedef enum
{
    PVT_DISABLE = 0, /* 禁用 */
    PVT_PV = 1,      /* PV */
    PVT_PT = 2,      /* PT */
} ePVTMode;

typedef enum
{
    TUNE_INIT = 0,       /* INIT */
    TUNE_IDLE = 1,       /* IDLE */
    TUNE_RESISTANCE = 2, /* 电阻校准 */
    TUNE_INDUCTANCE = 3, /* 电感校准 */
    TUNE_ENCODER = 4,    /* 编码器校准 */
    TUNE_ELEC_PARAM = 5, /* 电气参数校准 */
    TUNE_MECH_PARAM = 6, /* 机械参数校准 */
    TUNE_DONE = 7,       /* 完成 */
    TUNE_FAILED = 8,     /* 失败 */
} eTuneState;

typedef enum
{
    FOC_IDLE = 0,     /* IDLE */
    FOC_TUNE = 1,     /* TUNE */
    FOC_RESET = 2,    /* RESET */
    FOC_ENABLE = 3,   /* ENABLE */
    FOC_DISABLE = 4,  /* DISABLE */
    FOC_RUNNING = 5,  /* RUNNING */
    FOC_SHUTDOWN = 6, /* SHUTDOWN */
    FOC_FAULT = 7,    /* FAULT */
    FOC_WARNING = 8,  /* WARNING */
} eFocState;

typedef enum
{
    FAULT_NONE = 0,                   /* NONE */
    FAULT_FLASH_OFFLINE = 1,          /* FLASH离线 */
    FAULT_TUNE_CURRENT_VIBRATION = 2, /* 整定电流振荡 */
    FAULT_POLE_PAIR_MISMATCH = 3,     /* 极对数不匹配 */
    FAULT_MOTOR_LOCK = 4,             /* 电机堵转 */
    FAULT_RS_LS_CAL_FAIL = 5,         /* 内参校准失败 */
    FAULT_ENCODER_CAL_FAIL = 6,       /* 编码器校准失败 */
    FAULT_ELEC_PARAM_FAIL = 7,        /* 电气参数校准失败 */
    FAULT_MECH_PARAM_FAIL = 8,        /* 机械参数校准失败 */
    FAULT_OVERVOLTAGE = 9,            /* 过电压 */
    FAULT_UNDERVOLTAGE = 10,          /* 低电压 */
    FAULT_OVERCURRENT = 11,           /* 过电流 */
    FAULT_CAN_INIT_FAIL = 12,         /* CAN初始化失败 */
    FAULT_CAN_COMM_ERR = 13,          /* CAN通信异常 */
} eFaultState;

typedef enum
{
    WARNING_NONE = 0,             /* NONE */
    WARNING_OVERTEMP = 1,         /* 过温 */
    WARNING_OVERSPEED = 2,        /* 超速 */
    WARNING_POSITION_LIMIT = 3,   /* 位置超限 */
    WARNING_ENCODER_OFFLINE = 4,  /* 编码器无响应 */
    WARNING_ENCODER_COMM_ERR = 5, /* 编码器通信错误 */
} eWarningState;

typedef enum
{
    DRIVE_OFFLINE = 0, /* 离线 */
    DRIVE_ONLINE = 1,  /* 在线 */
    DRIVE_ERROR = 2,   /* 运行错误 */
    DRIVE_NORMAL = 3,  /* 运行正常 */
} eDriveState;

/* ---------- CMD ID ---------- */
#define UC_CONNECT 0xf0           /* 上位机连接 */
#define UC_DISCONNECT 0xfe        /* 上位机断开 */
#define START_TUNNING 0xf1        /* 开始调参 */
#define BRAKE 0xf2                /* 刹车 */
#define FOC_NRST 0xf3             /* FOC复位 */
#define CMD_ENABLE 0xf4           /* 电机使能 */
#define CMD_DISABLE 0xf5          /* 电机失能 */
#define LOG_GET 0xf7              /* 获取日志 */
#define LOG_ERASE 0xf8            /* 日志擦除 */
#define PARAM_ERASE 0x01          /* 参数擦除 */
#define PARAM_WRITE 0x02          /* 参数写入 */
#define PARAM_READ 0x03           /* 参数读取 */
#define PARAM_SAVE 0x04           /* 参数保存 */
#define CMD_REFVALUE_SET 0x21     /* 目标值设置 */
#define CMD_MODE_SET 0x22         /* 模式设置 */
#define CMD_STREAM_GET 0x23       /* 监测值获取 */
#define CMD_STREAM_SET 0x25       /* 数据流设置 */
#define CMD_SET_ZERO_POS 0x26     /* 设置零点 */
#define CMD_SET_LIMIT_POS 0x27    /* 设置极限位置 */
#define CMD_SYSTEM_RESET 0x30     /* 系统复位 */
#define CMD_IAP_ENTER 0x31        /* 进入IAP模式 */
#define CMD_IAP_ERASE_FLASH 0x32  /* 擦除Flash */
#define CMD_IAP_WRITE_FLASH 0x33  /* 写入Flash */
#define CMD_IAP_VERIFY_FLASH 0x34 /* 校验Flash */
#define CMD_IAP_EXIT 0x35         /* 退出IAP模式 */

/* ---------- 反馈 ID ---------- */
#define FEEDBACK_EXECUTE 0xf0 /* 成功 */
#define FEEDBACK_FAILURE 0xfe /* 失败 */

/* ---------- USB 协议格式 ---------- */
#define USB_PACKET_HEAD 0x55
#define USB_PACKET_TAIL 0xAA

/* ---------- UART 协议格式 ---------- */
#define UART_PACKET_HEAD 0x55
#define UART_PACKET_TAIL 0xAA

/* ---------- 帧长度 ---------- */
#define MAX_FRAME_LENGTH 128

#endif
