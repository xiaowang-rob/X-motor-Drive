/* 此文件由 protocol/gen_protocol.py 自动生成，请勿手动修改 */
#ifndef PROTO_CMD_LIST_H
#define PROTO_CMD_LIST_H

typedef enum
{
    CMD_CONNECT = 0xF0, /* 连接状态 */
    CMD_HEARTBEAT = 0xFB, /* 心跳请求；只回状态包，不与示波数据流耦合 */
    CMD_CORE_NRST = 0xFD, /* 核心复位 */
    CMD_SYSTEM_RESET = 0xFC, /* 系统复位 */
    CMD_RL_TUNNING = 0xE1, /* RL校准 */
    CMD_PSIF_TUNNING = 0xE2, /* Psif校准 */
    CMD_MECH_TUNNING = 0xE3, /* 机械参数校准 */
    CMD_OBS_TUNNING = 0xE4, /* 观测器校准 */
    CMD_PID_TUNNING = 0xE5, /* PID参数校准 */
    CMD_MIT_TUNNING = 0xE6, /* MIT参数校准 */
    CMD_PARAM_RELOAD = 0xC4, /* 参数重置；特殊 ID 见 parameter.yaml */
    CMD_PARAM_WRITE = 0xC5, /* 参数写入；特殊 ID 见 parameter.yaml，特殊 ID 不携带 value */
    CMD_PARAM_READ = 0xC6, /* 参数读取 */
    CMD_PARAM_SAVE = 0xC7, /* 参数保存 */
    CMD_DATA_ASK = 0xC8, /* 观测数据查询；回复解码需要请求的 id 上下文 */
    CMD_DATA_STREAM_SET = 0xC9, /* 示波数据流订阅，最多 5 路 */
    CMD_CTRL_CONFIG = 0xB0, /* 控制模式配置；首字节取自 obsdata 的 eMotorState，普通分支共 2 字节，MIT 分支共 8 字节 */
    CMD_OPENLOOP_CTRL = 0xB1, /* 开环控制；角度/速度模式由 CMD_CTRL_CONFIG 配置 */
    CMD_CUR_TAU_CTRL = 0xB3, /* 电流/力矩控制；电流/力矩模式由 CMD_CTRL_CONFIG 配置 */
    CMD_PID_VEL_CTRL = 0xB4, /* 速度PID控制 */
    CMD_PID_POS_CTRL = 0xB5, /* 位置PID控制 */
    CMD_MIT_CTRL = 0xB6, /* MIT控制；配置正指数 6/8，线上为 -6/-8 */
    CMD_DISABLE = 0xBB, /* 释放电机 */
    CMD_SET_ZERO_POS = 0xBD, /* 设置零点 */
    CMD_SET_LIMIT_POS = 0xBE, /* 以当前位置设置极限位置 */
    CMD_IAP_ENTER = 0xA1, /* 保存目标版本和大小后进入IAP模式；保存失败不得复位 */
    CMD_IAP_ERASE_FLASH = 0xA2, /* 擦除Flash */
    CMD_IAP_WRITE_FLASH = 0xA3, /* 写入Flash */
    CMD_IAP_VERIFY_FLASH = 0xA4, /* 校验Flash */
    CMD_IAP_EXIT = 0xA5, /* 退出IAP模式 */
    CMD_ASK_BL_VERSION = 0xA6, /* 查询bootloader版本 */
    CMD_ASK_APP_VERSION = 0xA7, /* 查询app版本 */
    CMD_ASK_DEVICE_MESSAGE = 0xA8, /* 查询设备信息 */
    CMD_DATA_STREAM = 0xCA, /* 示波数据流主动上报；由订阅决定内容 */
} eCmdId;

typedef enum
{
    UART_PACKET_HEAD = 0x81, /* 包头 */
    UART_PACKET_TAIL = 0x8E, /* 包尾 */
} eUartFrame;

typedef enum
{
    FB_EXECUTE = 0x91, /* 执行成功 */
    FB_FAILURE = 0x9E, /* 执行失败 */
} eFeedBack;

#endif
