/*文件由 gen_protocol.py 自动生成，请勿手动修改，相关配置在 protocol.json 中*/

#ifndef __PROTOCOL_H
#define __PROTOCOL_H
/*参数*/
typedef enum
{
    INC_ENC_MODE, /* 板载编码器模式 */
    INC_ENC_CHIP, // 板载编码器芯片
    EXT_ENC_MODE, /* 外接编码器 */
    EXT_ENC_CHIP, /* 外接编码器芯片 */
    ENC_ACTIVE,   // 编码器
    OBS_ACTIVE,   /* 无感观测 */
    CTRL_MODE,    /* 控制模式 */
    TRAJ_MODE,    /* 轨迹规划器 */

    MOTOR_POLEPAIRS, /* 电机极对数 */
    POSITIVE_DIR,    // 正方向
    THETA_OFFSET,    /* 角度补偿 */
    MOTOR_KV,        /* KV */
    MOTOR_RS,        /* 相电阻 */
    MOTOR_Ld,        /* Ld */
    MOTOR_Lq,        /* Lq */
    MOTOR_PSIF,      /* 磁链 */
    MOTOR_KE,        /* 反电动势常数 */
    MOTOR_J,         /* 转动惯量 */
    MOTOR_B,         /* 摩擦系数 */

    CAN_ID,   /* CAN ID */
    CAN_MODE, /* CAN模式 */
    CLBW,     // 电流环带宽
    QCLKP,    // q轴电流环比例
    QCLKI,    // q轴电流环积分
    DCLKP,    // d轴电流环比例
    DCLKI,    // d轴电流环积分
    CFALPHA,  // 电流滤波系数
    VLKP,     /* 速度环比例 */
    VLKI,     /* 速度环积分 */
    WLKP,     // 弱磁环比例
    WLKI,     /* 弱磁环积分 */
    PLKP,     /* 位置环比例 */
    PLKI,     /* 位置环积分 */
    PLKD,     /* 位置环微分 */
    PLALPHA,  /* 位置环滤波系数 */
    MIT_KP,   /* MIT刚度 */
    MIT_KD,   /* MIT阻尼 */
    MIT_TSTA, // MIT静态扭矩补偿
    MIT_TMAX, /* MIT最大扭矩 */

    TUNE_CURRENT,       /* 校准电流 */
    LIMIT_CURRENT,      /* 电流限幅 */
    LIMIT_VELOCITY,     /* 速度限幅 */
    LIMIT_POSITION_MIN, /* 位置限幅最小值 */
    LIMIT_POSITION_MAX, /* 位置限幅最大值 */
    TOLERANCE_TIME,     /* 容忍时间 */
    TOLERANCE_LIMIT,    /* 超限容忍度 */

    TRAJ_LIMIT_D1,  /* 一阶限幅 */
    TRAJ_LIMIT_D2,  /* 二阶限幅 */
    TRAJ_LIMIT_D3,  /* 三阶限幅 */
    TRAJ_TOLERANCE, /* 轨迹规划容差 */
    PARAM_NUM,
} eParameter;
/*数据*/
typedef enum
{
    CURRENT_U,     /* I_u */
    CURRENT_V,     /* I_v */
    CURRENT_W,     /* I_w */
    VOLTAGE_Q,     /* V_q */
    VOLTAGE_D,     /* V_d */
    CURRENT_ALPHA, /* I_α */
    CURRENT_BETA,  /* I_β */
    CURRENT_Q,     /* I_q */
    CURRENT_D,     /* I_d */
    CURRENT_Q_REF, /* Iq_ref */
    CURRENT_D_REF, /* Id_ref */
    VELOCITY,      /* vel */
    VELOCITY_REF,  /* vel_ref */
    THETA_ELEC,    /* theta_e */
    THETA_MECH,    /* theta_m */
    POSITION,      /* pos */
    POSITION_REF,  /* pos_ref */
    DATA_NUM
} eDataList;
typedef enum
{
    LOG_HOURS,   // 时
    LOG_MINUTES, // 分
    LOG_FAULT,   // 故障
    LOG_WARNING, // 警告`
    LOG_VBUS,    // 电压
    LOG_TEMP,    // 温度
    LOG_IU,      // U 电流
    LOG_IV,      // V 电流
    LOG_IW,      // W 电流
    LOG_ID,      // D 电流
    LOG_ID_TAG,  // D 电流目标
    LOG_IQ,      // Q 电流
    LOG_IQ_TAG,  // Q 电流目标
    LOG_VEL,     // 速度
    LOG_VEL_TAG, // 速度目标
    LOG_POS,     // 位置
    LOG_POS_TAG, // 位置目标
    LOG_NUM
} eLogList;
/*编码器模式*/
typedef enum
{
    ENC_OFF, // 禁用
    ENC_ON,  // 实时角度
    ENC_PLL, // pll跟踪角度
} eENCmode;  // 编码器模式
/*编码器型号*/
typedef enum
{
    ENC_NONE, /* 无 */
    MT6816,   /* MT6816 */
    MT6835,   /* MT6835 */
    AS5047,   /* AS5047 */
} eEncoderChip;
/*观测模式*/
typedef enum
{
    OPENLOOP_MODE, /* 无感开环 */
    ENCODER_MODE,  /* 编码反馈 */
    OBSERVER_MODE, /* 无感观测器 */
    MERGE_MODE,    /* 融合模式 */
} eObsMode;
/*运行模式*/
typedef enum
{
    CURRENT_MODE, /* 电流模式 */
    PID_SPEED,    /* PID速度模式 */
    PID_POSITION, /* PID位置模式 */
    MIT_MODE,     /* MIT模式 */
} eCtrlMode;
/*CAN模式*/
typedef enum
{
    CAN_SILENCE, /* 静默模式 */
    CAN_ANSWER,  /* 响应模式 */
} eCanMode;
/*PVT模式*/
typedef enum
{
    PVT_DISABLE, /* 禁用 */
    PVT_PV,      /* PV */
    PVT_PT,      /* PT */
} ePVTMode;
/*参数校准状态*/
typedef enum
{
    TUNE_INIT,       /* INIT */
    TUNE_IDLE,       /* IDLE */
    TUNE_RESISTANCE, /* 电阻校准 */
    TUNE_INDUCTANCE, /* 电感校准 */
    TUNE_ENCODER,    /* 编码器校准 */
    TUNE_ELEC_PARAM, /* 电气参数校准 */
    TUNE_MECH_PARAM, /* 机械参数校准 */
    TUNE_DONE,       /* 完成 */
    TUNE_FAILED,     /* 失败 */
} eTuneState;
// 核心状态
typedef enum
{
    INIT,    // INIT
    IDLE,    // IDLE
    TUNING,  // TUNING
    RUNNING, // RUNNING
    FAULT,   // FAULT
    WARNING, // WARNING
} eCoreState;
/*错误*/
typedef enum
{
    FAULT_NONE,                   /* NONE */
    FAULT_FLASH_OFFLINE,          /* FLASH离线 */
    FAULT_TUNE_CURRENT_VIBRATION, /* 整定电流振荡 */
    FAULT_POLE_PAIR_MISMATCH,     /* 极对数不匹配 */
    FAULT_MOTOR_LOCK,             /* 电机堵转 */
    FAULT_RS_LS_CAL_FAIL,         /* 内参校准失败 */
    FAULT_ENCODER_CAL_FAIL,       /* 编码器校准失败 */
    FAULT_ELEC_PARAM_FAIL,        /* 电气参数校准失败 */
    FAULT_MECH_PARAM_FAIL,        /* 机械参数校准失败 */
    FAULT_OVERVOLTAGE,            /* 过电压 */
    FAULT_UNDERVOLTAGE0,          /* 低电压 */
    FAULT_OVERCURRENT1,           /* 过电流 */
    FAULT_CAN_INIT_FAIL2,         /* CAN初始化失败 */
    FAULT_CAN_COMM_ERR3,          /* CAN通信异常 */
} eFault;
/*警告*/
typedef enum
{
    WARNING_NONE,             /* NONE */
    WARNING_OVERTEMP,         /* 过温 */
    WARNING_OVERSPEED,        /* 超速 */
    WARNING_POSITION_LIMIT,   /* 位置超限 */
    WARNING_ENCODER_OFFLINE,  /* 编码器无响应 */
    WARNING_ENCODER_COMM_ERR, /* 编码器通信错误 */
} eWarning;

/*  CMD ID  */
#define CMD_CONNECT 0xf0       /* 上位机连接 */
#define CMD_DISCONNECT 0xfe    /* 上位机断开 */
#define CMD_START_TUNNING 0xf1 /* 开始调参 */
#define CMD_CORE_NRST 0xf3     /* 核心复位 */
#define CMD_LOG_GET 0xf7       /* 获取日志 */
#define CMD_PARAM_RELOAD 0x01  /* 参数重置 */
#define CMD_PARAM_WRITE 0x02   /* 参数写入 */
#define CMD_PARAM_READ 0x03    /* 参数读取 */
#define CMD_PARAM_SAVE 0x04    /* 参数保存 */

#define CMD_ENABLE 0xf4        /* 电机使能 */
#define CMD_DISABLE 0xf5       /* 电机失能 */
#define CMD_TARGET_SET 0x21    /* 目标值设置 */
#define CMD_MODE_SET 0x22      /* 控制模式设置 */
#define CMD_DATA_ASK 0x23      /* 数据查询 */
#define CMD_STREAM_SET 0x25    /* 数据流设置 */
#define CMD_SET_ZERO_POS 0x26  /* 设置零点 */
#define CMD_SET_LIMIT_POS 0x27 /* 设置极限位置 */

#define CMD_SYSTEM_RESET 0x30     /* 系统复位 */
#define CMD_IAP_ENTER 0x31        /* 进入IAP模式 */
#define CMD_IAP_ERASE_FLASH 0x32  /* 擦除Flash */
#define CMD_IAP_WRITE_FLASH 0x33  /* 写入Flash */
#define CMD_IAP_VERIFY_FLASH 0x34 /* 校验Flash */
#define CMD_IAP_EXIT 0x35         /* 退出IAP模式 */

/*  反馈 ID  */
#define FB_EXECUTE 0xf0 /* 成功 */
#define FB_FAILURE 0xfe /* 失败 */

/*  USBcdc/UART 协议格式  */
#define UART_PACKET_HEAD 0x55
#define UART_PACKET_TAIL 0xAA

#endif
