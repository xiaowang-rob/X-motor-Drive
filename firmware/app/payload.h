/* APP 命令的 payload 编解码，手工维护；不由 YAML 生成。
 * 多字节均为小端，宏返回固定 payload 长度，由调用方校验接收长度。
 * 参数、观测数据流等动态 payload 仍在对应业务 handler 中处理。 */
#ifndef APP_PAYLOAD_H
#define APP_PAYLOAD_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cmd_list.h"
#include "data_list.h"

/* 基础读写避免未对齐访问，不依赖结构体布局。 */
static inline uint16_t payload_u16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static inline uint32_t payload_u24(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

static inline uint32_t payload_u32(const uint8_t *p) {
    return payload_u24(p) | ((uint32_t)p[3] << 24);
}

static inline int32_t payload_i24(const uint8_t *p) {
    uint32_t raw = payload_u24(p);
    return (raw & 0x800000U) ? (int32_t)raw - 0x1000000 : (int32_t)raw;
}

static inline int32_t payload_i16(const uint8_t *p) {
    uint16_t raw = payload_u16(p);
    return (raw & 0x8000U) ? (int32_t)raw - 0x10000 : (int32_t)raw;
}

static inline float payload_f32(const uint8_t *p) {
    uint32_t raw = payload_u32(p);
    float value;
    memcpy(&value, &raw, sizeof(value));
    return value;
}

static inline void payload_put_u32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static inline double payload_pow2(int exponent) {
    double factor = 1.0;
    for (int i = 0; i < exponent; ++i)
        factor *= 2.0;
    for (int i = 0; i > exponent; --i)
        factor *= 0.5;
    return factor;
}

static inline int payload_i4(uint8_t nibble) {
    int raw = nibble & 0x0F;
    return (raw & 8) ? raw - 16 : raw;
}

#define ENPACK_FEEDBACK(_out_length, _packet, _success)                        \
    do {                                                                       \
        ((uint8_t *)(_packet))[0] = (_success) ? FB_EXECUTE : FB_FAILURE;      \
        (_out_length) = 1;                                                     \
    } while (0)

/* 心跳：[state:u8][fault:u8][vbus:u16 / 2^8][driver_temp:u8][motor_temp:u8] */
#define ENPACK_HEARTBEAT_RESPONSE(                                             \
    _out_length, _packet, _state, _fault, _vbus, _driver_temp, _motor_temp)    \
    do {                                                                       \
        uint8_t *_payload_out = (_packet);                                     \
        double _payload_voltage = (double)(_vbus) * 256.0 + 0.5;               \
        uint16_t _payload_vbus = (uint16_t)_payload_voltage;                   \
        _payload_out[0] = (uint8_t)(_state);                                   \
        _payload_out[1] = (uint8_t)(_fault);                                   \
        _payload_out[2] = (uint8_t)_payload_vbus;                              \
        _payload_out[3] = (uint8_t)(_payload_vbus >> 8);                       \
        _payload_out[4] = (uint8_t)((_driver_temp) + 0.5f);                    \
        _payload_out[5] = (uint8_t)((_motor_temp) + 0.5f);                     \
        (_out_length) = 6;                                                     \
    } while (0)

#define DEPACK_CONNECT_REQUEST(_out_length, _packet, _state)                   \
    do {                                                                       \
        (_state) = ((const uint8_t *)(_packet))[0];                            \
        (_out_length) = 1;                                                     \
    } while (0)

/* 校准：RL / Psif 为单个 f32，其余为 [current:f32][velocity:f32]。 */
#define DEPACK_RL_TUNNING_REQUEST(_out_length, _packet, _tune_current)         \
    do {                                                                       \
        (_tune_current) = payload_f32(_packet);                                \
        (_out_length) = 4;                                                     \
    } while (0)

#define DEPACK_PSIF_TUNNING_REQUEST(_out_length, _packet, _tune_vel)           \
    do {                                                                       \
        (_tune_vel) = payload_f32(_packet);                                    \
        (_out_length) = 4;                                                     \
    } while (0)

#define DEPACK_MECH_TUNNING_REQUEST(                                           \
    _out_length, _packet, _tune_current, _tune_vel)                            \
    do {                                                                       \
        const uint8_t *_payload_in = (_packet);                                \
        (_tune_current) = payload_f32(_payload_in);                            \
        (_tune_vel) = payload_f32(_payload_in + 4);                            \
        (_out_length) = 8;                                                     \
    } while (0)

#define DEPACK_OBS_TUNNING_REQUEST(                                            \
    _out_length, _packet, _tune_current, _tune_vel)                            \
    DEPACK_MECH_TUNNING_REQUEST(_out_length, _packet, _tune_current, _tune_vel)
#define DEPACK_PID_TUNNING_REQUEST(                                            \
    _out_length, _packet, _tune_current, _tune_vel)                            \
    DEPACK_MECH_TUNNING_REQUEST(_out_length, _packet, _tune_current, _tune_vel)
#define DEPACK_MIT_TUNNING_REQUEST(                                            \
    _out_length, _packet, _tune_current, _tune_vel)                            \
    DEPACK_MECH_TUNNING_REQUEST(_out_length, _packet, _tune_current, _tune_vel)

#define DEPACK_PARAM_RELOAD_REQUEST(_out_length, _packet, _id)                 \
    do {                                                                       \
        (_id) = ((const uint8_t *)(_packet))[0];                               \
        (_out_length) = 1;                                                     \
    } while (0)
#define DEPACK_PARAM_READ_REQUEST(_out_length, _packet, _id)                   \
    DEPACK_PARAM_RELOAD_REQUEST(_out_length, _packet, _id)
#define DEPACK_DATA_ASK_REQUEST(_out_length, _packet, _id)                     \
    DEPACK_PARAM_RELOAD_REQUEST(_out_length, _packet, _id)

/* 普通模式：[mode:u8][control:u8]；MIT：[mode:u8][scale:u8][kp:u24][kd:u24]。
 */
#define DEPACK_CTRL_MODE_REQUEST(_packet, _mode)                               \
    do {                                                                       \
        uint8_t _payload_mode = ((const uint8_t *)(_packet))[0];               \
        (_mode) = _payload_mode;                                               \
    } while (0)

#define DEPACK_CTRL_CONFIG_REQUEST(_packet, _control)                          \
    do {                                                                       \
        (_control) = ((const uint8_t *)(_packet))[1];                          \
    } while (0)

#define DEPACK_CTRL_CONFIG_MIT_REQUEST(_packet, _kp, _kd)                      \
    do {                                                                       \
        const uint8_t *_payload_in = (_packet);                                \
        double _payload_scale = payload_pow2(-(int)_payload_in[1]);            \
        (_kp) = payload_u24(_payload_in + 2) * _payload_scale;                 \
        (_kd) = payload_u24(_payload_in + 5) * _payload_scale;                 \
    } while (0)

/* 控制目标：开环 [duty:u16 / 2^8][value:f32]，电流/速度/位置各为 f32。 */
#define DEPACK_OPENLOOP_CTRL_REQUEST(_out_length, _packet, _duty, _value)      \
    do {                                                                       \
        const uint8_t *_payload_in = (_packet);                                \
        (_duty) = payload_u16(_payload_in) / 256.0f;                           \
        (_value) = payload_f32(_payload_in + 2);                               \
        (_out_length) = 6;                                                     \
    } while (0)

#define DEPACK_CUR_TAU_CTRL_REQUEST(_out_length, _packet, _value)              \
    do {                                                                       \
        (_value) = payload_f32(_packet);                                       \
        (_out_length) = 4;                                                     \
    } while (0)
#define DEPACK_PID_VEL_CTRL_REQUEST(_out_length, _packet, _velocity)           \
    DEPACK_CUR_TAU_CTRL_REQUEST(_out_length, _packet, _velocity)
#define DEPACK_PID_POS_CTRL_REQUEST(_out_length, _packet, _position)           \
    DEPACK_CUR_TAU_CTRL_REQUEST(_out_length, _packet, _position)

/* MIT：[position:i24 / 2^18][velocity_exp:i4 |
 * torque_exp:i4][velocity:i16][torque:i16] 半字节存负指数，工程值 = raw *
 * 2^(线上指数)。 */
#define DEPACK_MIT_CTRL_REQUEST(                                               \
    _out_length, _packet, _position, _velocity, _torque)                       \
    do {                                                                       \
        const uint8_t *_payload_in = (_packet);                                \
        (_position) = payload_i24(_payload_in) / 262144.0f;                    \
        (_velocity) = payload_i16(_payload_in + 4) *                           \
                      payload_pow2(payload_i4(_payload_in[3]));                \
        (_torque) = payload_i16(_payload_in + 6) *                             \
                    payload_pow2(payload_i4(_payload_in[3] >> 4));             \
        (_out_length) = 8;                                                     \
    } while (0)

/* IAP：u32 字段均为小端。 */
#define DEPACK_IAP_ENTER_REQUEST(_out_length, _packet, _version, _size)        \
    do {                                                                       \
        const uint8_t *_payload_in = (_packet);                                \
        (_version) = payload_u32(_payload_in);                                 \
        (_size) = payload_u32(_payload_in + 4);                                \
        (_out_length) = 8;                                                     \
    } while (0)

#define DEPACK_IAP_ERASE_FLASH_REQUEST(_out_length, _packet, _size)            \
    do {                                                                       \
        (_size) = payload_u32(_packet);                                        \
        (_out_length) = 4;                                                     \
    } while (0)

#define ENPACK_IAP_WRITE_FLASH_RESPONSE(                                       \
    _out_length, _packet, _offset, _status)                                    \
    do {                                                                       \
        uint8_t *_payload_out = (_packet);                                     \
        payload_put_u32(_payload_out, (_offset));                              \
        _payload_out[4] = (uint8_t)(_status);                                  \
        (_out_length) = 5;                                                     \
    } while (0)

#define DEPACK_IAP_VERIFY_FLASH_REQUEST(_out_length, _packet, _crc32, _size)   \
    do {                                                                       \
        const uint8_t *_payload_in = (_packet);                                \
        (_crc32) = payload_u32(_payload_in);                                   \
        (_size) = payload_u32(_payload_in + 4);                                \
        (_out_length) = 8;                                                     \
    } while (0)

#endif
