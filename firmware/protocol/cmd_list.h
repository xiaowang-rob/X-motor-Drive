/* 此文件由 protocol/gen_protocol.py 自动生成，请勿手动修改 */

#ifndef __CMD_LIST_H
#define __CMD_LIST_H

// CMD ID
typedef enum
{
    CMD_CONNECT = 0xF0,            // 连接状态 1/0
    CMD_HEARTBEAT = 0xFB,          // 心跳/数据流请求
    CMD_CORE_NRST = 0xFD,          // 核心复位
    CMD_SYSTEM_RESET = 0xFC,       // 系统复位
    CMD_RL_TUNNING = 0xE1,         // RL校准
    CMD_PSIF_TUNNING = 0xE2,       // Psif校准
    CMD_MECH_TUNNING = 0xE3,       // 机械参数校准
    CMD_OBS_TUNNING = 0xE4,        // 观测器校准
    CMD_PID_TUNNING = 0xE5,        // PID参数校准
    CMD_MIT_TUNNING = 0xE6,        // MIT参数校准
    CMD_PARAM_ALLRELOAD = 0xC1,    // 参数一键重置
    CMD_PARAM_ALLWRITE = 0xC2,     // 参数一键写入
    CMD_PARAM_ALLREAD = 0xC3,      // 参数一键读取
    CMD_PARAM_RELOAD = 0xC4,       // 参数重置
    CMD_PARAM_WRITE = 0xC5,        // 参数写入
    CMD_PARAM_READ = 0xC6,         // 参数读取
    CMD_PARAM_SAVE = 0xC7,         // 参数保存
    CMD_DATA_ASK = 0xC8,           // 数据查询
    CMD_DATA_STREAM_SET = 0xC9,    // 数据流设置
    CMD_OPENLOOP_THETA = 0xB1,     // 开环角度控制
    CMD_OPENLOOP_VEL = 0xB2,       // 开环速度控制
    CMD_CUR_TAU_MODE = 0xB3,       // 电流/力矩控制
    CMD_PID_VEL = 0xB4,            // 速度PID控制
    CMD_PID_POS = 0xB5,            // 位置PID控制
    CMD_MIT_POS = 0xB6,            // MIT控制
    CMD_MIT_CONFIG = 0xB7,         // MITKpKd配置
    CMD_MIT_VT = 0xB8,             // MIT控制补充VT
    CMD_DISABLE = 0xBB,            // 释放电机
    CMD_SET_ZERO_POS = 0xBD,       // 设置零点
    CMD_SET_LIMIT_POS = 0xBE,      // 设置极限位置
    CMD_IAP_ENTER = 0xA1,          // 进入IAP模式
    CMD_IAP_ERASE_FLASH = 0xA2,    // 擦除Flash
    CMD_IAP_WRITE_FLASH = 0xA3,    // 写入Flash
    CMD_IAP_VERIFY_FLASH = 0xA4,   // 校验Flash
    CMD_IAP_EXIT = 0xA5,           // 退出IAP模式
    CMD_ASK_BL_VERSION = 0xA6,     // 查询bootload版本
    CMD_ASK_APP_VERSION = 0xA7,    // 查询app版本
    CMD_ASK_DEVICE_MESSAGE = 0xA8  // 查询设备信息
} eCmdId;

// 反馈 ID
typedef enum
{
    FB_EXECUTE = 0x90, // 成功
    FB_FAILURE = 0x9E  // 失败
} eFbId;

// USBcdc/UART 协议格式
typedef enum
{
    UART_PACKET_HEAD = 0x81, // 包头
    UART_PACKET_TAIL = 0x8E  // 包尾
} eUartFrame;

// 控制目标
typedef enum
{
    TARGET_THETA_E = 0x71,    // 电角度
    TARGET_VEL_E = 0x72,      // 电角速度
    TARGET_DUTY = 0x73,       // 占空比
    TARGET_CUR_Q = 0x74,      // 拖动电流
    TARGET_TAU = 0x75,        // 转矩
    TARGET_VEL = 0x76,        // 速度
    TARGET_POS = 0x77,        // 位置
    TARGET_PVT_TP = 0x78,     // PVT时间点
    TARGET_MIT_TAU_FF = 0x78  // MIT前馈力矩
} eCmdTarget;

#endif
