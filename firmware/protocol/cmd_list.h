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
    CMD_PARAM_RELOAD = 0xC4, /* 参数重置；0xFF 表示全部重置 */
    CMD_PARAM_WRITE = 0xC5, /* 参数写入；0xFF 表示应用已写入参数 */
    CMD_PARAM_READ = 0xC6, /* 参数读取 */
    CMD_PARAM_SAVE = 0xC7, /* 参数保存 */
    CMD_DATA_ASK = 0xC8, /* 观测数据查询；回复解码需要请求的 id 上下文 */
    CMD_DATA_STREAM_SET = 0xC9, /* 示波数据流订阅，最多 5 路 */
    CMD_OPENLOOP_CTRL = 0xB1, /* 开环控制 */
    CMD_CUR_TAU_CTRL = 0xB3, /* 电流/力矩控制 */
    CMD_PID_VEL_CTRL = 0xB4, /* 速度PID控制 */
    CMD_PID_POS_CTRL = 0xB5, /* 位置PID控制 */
    CMD_MIT_CTRL = 0xB6, /* MIT控制；配置正指数 6/8，线上为 -6/-8 */
    CMD_MIT_CONFIG = 0xB7, /* MIT Kp/Kd配置 */
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

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define ENPACK_FEEDBACK(_out_length, _packet, _success) \
    do { \
        ((uint8_t *)(_packet))[0] = (_success) ? FB_EXECUTE : FB_FAILURE; \
        (_out_length) = 1; \
    } while (0)

#define DEPACK_CONNECT_REQUEST(_out_length, _packet, state) \
    do { \
        const uint8_t *_proto_packet = (_packet); \
        uint8_t _proto_state; \
        memcpy(&_proto_state, _proto_packet + 0, 1); \
        (state) = _proto_state; \
        (_out_length) = 1; \
    } while (0)

#define ENPACK_HEARTBEAT_RESPONSE(_out_length, _packet, state, fault, warning, vbus, driver_temp, motor_temp) \
    do { \
        uint8_t *_proto_packet = (_packet); \
        uint8_t _proto_state = (uint8_t)(state); \
        memcpy(_proto_packet + 0, &_proto_state, 1); \
        uint8_t _proto_fault = (uint8_t)(fault); \
        memcpy(_proto_packet + 1, &_proto_fault, 1); \
        uint8_t _proto_warning = (uint8_t)(warning); \
        memcpy(_proto_packet + 2, &_proto_warning, 1); \
        double _proto_scaled_vbus = (double)(vbus) * 256; \
        uint16_t _proto_vbus = (uint16_t)(_proto_scaled_vbus >= 0 ? _proto_scaled_vbus + 0.5 : _proto_scaled_vbus - 0.5); \
        memcpy(_proto_packet + 3, &_proto_vbus, 2); \
        uint8_t _proto_driver_temp = (uint8_t)(driver_temp); \
        memcpy(_proto_packet + 5, &_proto_driver_temp, 1); \
        uint8_t _proto_motor_temp = (uint8_t)(motor_temp); \
        memcpy(_proto_packet + 6, &_proto_motor_temp, 1); \
        (_out_length) = 7; \
    } while (0)

#define DEPACK_PARAM_READ_REQUEST(_out_length, _packet, id) \
    do { \
        const uint8_t *_proto_packet = (_packet); \
        uint8_t _proto_id; \
        memcpy(&_proto_id, _proto_packet + 0, 1); \
        (id) = _proto_id; \
        (_out_length) = 1; \
    } while (0)

#define DEPACK_DATA_ASK_REQUEST(_out_length, _packet, id) \
    do { \
        const uint8_t *_proto_packet = (_packet); \
        uint8_t _proto_id; \
        memcpy(&_proto_id, _proto_packet + 0, 1); \
        (id) = _proto_id; \
        (_out_length) = 1; \
    } while (0)

#define DEPACK_OPENLOOP_CTRL_REQUEST(_out_length, _packet, mode, duty, value) \
    do { \
        const uint8_t *_proto_packet = (_packet); \
        uint8_t _proto_mode; \
        memcpy(&_proto_mode, _proto_packet + 0, 1); \
        uint16_t _proto_duty; \
        memcpy(&_proto_duty, _proto_packet + 1, 2); \
        float _proto_value; \
        memcpy(&_proto_value, _proto_packet + 3, 4); \
        (mode) = _proto_mode; \
        (duty) = (float)_proto_duty / 256; \
        (value) = _proto_value; \
        (_out_length) = 7; \
    } while (0)

#define DEPACK_CUR_TAU_CTRL_REQUEST(_out_length, _packet, mode, value) \
    do { \
        const uint8_t *_proto_packet = (_packet); \
        uint8_t _proto_mode; \
        memcpy(&_proto_mode, _proto_packet + 0, 1); \
        float _proto_value; \
        memcpy(&_proto_value, _proto_packet + 1, 4); \
        (mode) = _proto_mode; \
        (value) = _proto_value; \
        (_out_length) = 5; \
    } while (0)

#define DEPACK_PID_VEL_CTRL_REQUEST(_out_length, _packet, velocity) \
    do { \
        const uint8_t *_proto_packet = (_packet); \
        float _proto_velocity; \
        memcpy(&_proto_velocity, _proto_packet + 0, 4); \
        (velocity) = _proto_velocity; \
        (_out_length) = 4; \
    } while (0)

#define DEPACK_PID_POS_CTRL_REQUEST(_out_length, _packet, position) \
    do { \
        const uint8_t *_proto_packet = (_packet); \
        float _proto_position; \
        memcpy(&_proto_position, _proto_packet + 0, 4); \
        (position) = _proto_position; \
        (_out_length) = 4; \
    } while (0)

#define DEPACK_MIT_CTRL_REQUEST(_out_length, _packet, position, velocity_scale, torque_scale, velocity, torque) \
    do { \
        const uint8_t *_proto_packet = (_packet); \
        int32_t _proto_position = (int32_t)((uint32_t)_proto_packet[0] | \
            ((uint32_t)_proto_packet[1] << 8) | ((uint32_t)_proto_packet[2] << 16)); \
        if (_proto_position & 0x800000) _proto_position -= 0x1000000; \
        int8_t _proto_velocity_scale = (_proto_packet[3] >> 0) & 15; \
        if (_proto_velocity_scale & 8) _proto_velocity_scale -= 16; \
        int8_t _proto_torque_scale = (_proto_packet[3] >> 4) & 15; \
        if (_proto_torque_scale & 8) _proto_torque_scale -= 16; \
        int16_t _proto_velocity; \
        memcpy(&_proto_velocity, _proto_packet + 4, 2); \
        int16_t _proto_torque; \
        memcpy(&_proto_torque, _proto_packet + 6, 2); \
        (position) = (float)_proto_position / 262144; \
        (velocity_scale) = -_proto_velocity_scale; \
        (torque_scale) = -_proto_torque_scale; \
        (velocity) = (_proto_velocity_scale <= 0 ? (float)_proto_velocity / (1u << -_proto_velocity_scale) : (float)_proto_velocity * (1u << _proto_velocity_scale)); \
        (torque) = (_proto_torque_scale <= 0 ? (float)_proto_torque / (1u << -_proto_torque_scale) : (float)_proto_torque * (1u << _proto_torque_scale)); \
        (_out_length) = 8; \
    } while (0)

#define DEPACK_MIT_CONFIG_REQUEST(_out_length, _packet, kp, kd) \
    do { \
        const uint8_t *_proto_packet = (_packet); \
        float _proto_kp; \
        memcpy(&_proto_kp, _proto_packet + 0, 4); \
        float _proto_kd; \
        memcpy(&_proto_kd, _proto_packet + 4, 4); \
        (kp) = _proto_kp; \
        (kd) = _proto_kd; \
        (_out_length) = 8; \
    } while (0)

#define DEPACK_IAP_ENTER_REQUEST(_out_length, _packet, version, size) \
    do { \
        const uint8_t *_proto_packet = (_packet); \
        uint32_t _proto_version; \
        memcpy(&_proto_version, _proto_packet + 0, 4); \
        uint32_t _proto_size; \
        memcpy(&_proto_size, _proto_packet + 4, 4); \
        (version) = _proto_version; \
        (size) = _proto_size; \
        (_out_length) = 8; \
    } while (0)

#define DEPACK_IAP_ERASE_FLASH_REQUEST(_out_length, _packet, size) \
    do { \
        const uint8_t *_proto_packet = (_packet); \
        uint32_t _proto_size; \
        memcpy(&_proto_size, _proto_packet + 0, 4); \
        (size) = _proto_size; \
        (_out_length) = 4; \
    } while (0)

#define ENPACK_IAP_WRITE_FLASH_RESPONSE(_out_length, _packet, offset, status) \
    do { \
        uint8_t *_proto_packet = (_packet); \
        uint32_t _proto_offset = (uint32_t)(offset); \
        memcpy(_proto_packet + 0, &_proto_offset, 4); \
        uint8_t _proto_status = (uint8_t)(status); \
        memcpy(_proto_packet + 4, &_proto_status, 1); \
        (_out_length) = 5; \
    } while (0)

#define DEPACK_IAP_VERIFY_FLASH_REQUEST(_out_length, _packet, crc32, size) \
    do { \
        const uint8_t *_proto_packet = (_packet); \
        uint32_t _proto_crc32; \
        memcpy(&_proto_crc32, _proto_packet + 0, 4); \
        uint32_t _proto_size; \
        memcpy(&_proto_size, _proto_packet + 4, 4); \
        (crc32) = _proto_crc32; \
        (size) = _proto_size; \
        (_out_length) = 8; \
    } while (0)

#endif
