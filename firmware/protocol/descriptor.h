/* 此文件由 protocol/gen_protocol.py 自动生成，请勿手动修改 */
#ifndef PROTO_DESCRIPTOR_H
#define PROTO_DESCRIPTOR_H

#include "fieldvaluetype.h"

#include <stdint.h>
#include <stdbool.h>

#define PROTO_STR_CONTROL "control"
#define PROTO_STR_COUNT "count"
#define PROTO_STR_CRC32 "crc32"
#define PROTO_STR_DATA "data"
#define PROTO_STR_DRIVER_TEMP "driver_temp"
#define PROTO_STR_DUTY "duty"
#define PROTO_STR_FAULT "fault"
#define PROTO_STR_ID "id"
#define PROTO_STR_IDS "ids"
#define PROTO_STR_KD "kd"
#define PROTO_STR_KP "kp"
#define PROTO_STR_MODE "mode"
#define PROTO_STR_MOTOR_TEMP "motor_temp"
#define PROTO_STR_OFFSET "offset"
#define PROTO_STR_POSITION "position"
#define PROTO_STR_SCALE "scale"
#define PROTO_STR_SIZE "size"
#define PROTO_STR_STATE "state"
#define PROTO_STR_STATUS "status"
#define PROTO_STR_TEXT "text"
#define PROTO_STR_TORQUE "torque"
#define PROTO_STR_TORQUE_SCALE "torque_scale"
#define PROTO_STR_TUNE_CURRENT "tune_current"
#define PROTO_STR_TUNE_VEL "tune_vel"
#define PROTO_STR_VALUE "value"
#define PROTO_STR_VALUES "values"
#define PROTO_STR_VBUS "vbus"
#define PROTO_STR_VELOCITY "velocity"
#define PROTO_STR_VELOCITY_SCALE "velocity_scale"
#define PROTO_STR_VERSION "version"

#define PROTO_TYPE_DYNAMIC ((eFieldValueType)255)

typedef enum
{
    PROTO_SCALE_NONE = 0,   /* 直接写原始值 */
    PROTO_SCALE_POW2,       /* raw = round(value * 2^exponent) */
    PROTO_SCALE_SIGNED_EXP, /* FLD_I4 存负指数：value = raw * 2^raw */
    PROTO_SCALE_REF_U24,    /* 前置 U8 指数，无符号 24 位 */
    PROTO_SCALE_REF_EXP,    /* 前置 FLD_I4 指数，有符号 16 位 */
} eProtoScale;

typedef struct
{
    const char *name;
    eFieldValueType type;
    uint8_t offset;
    uint8_t size;   /* 0 表示由上下文决定 */
    uint8_t lo;     /* FLD_I4 位域低位 */
    uint8_t hi;     /* FLD_I4 位域高位；0 表示整字节 */
    eProtoScale scale;
    int8_t exponent; /* PROTO_SCALE_POW2 的指数 */
    uint8_t ref;     /* 指数字段在所属布局内的下标 */
    bool saturate;
} tProtoField;

typedef struct
{
    uint8_t cmd;       /* 命令 ID */
    uint8_t direction; /* 0 请求，1 回复 */
    uint16_t start;    /* 字段在 g_proto_fields 中的起点 */
    uint8_t count;
    uint8_t fixed;     /* 固定部分字节数 */
    bool dynamic;
} tProtoLayout;

typedef struct
{
    const tProtoLayout *base;
    uint8_t selector; /* 首字节模式值 */
    const tProtoLayout *variant;
} tProtoVariant;

typedef enum
{
    PROTO_LAYOUT_CMD_CONNECT_REQUEST,
    PROTO_LAYOUT_CMD_CONNECT_RESPONSE,
    PROTO_LAYOUT_CMD_HEARTBEAT_RESPONSE,
    PROTO_LAYOUT_CMD_CORE_NRST_RESPONSE,
    PROTO_LAYOUT_CMD_RL_TUNNING_REQUEST,
    PROTO_LAYOUT_CMD_PSIF_TUNNING_REQUEST,
    PROTO_LAYOUT_CMD_MECH_TUNNING_REQUEST,
    PROTO_LAYOUT_CMD_OBS_TUNNING_REQUEST,
    PROTO_LAYOUT_CMD_PID_TUNNING_REQUEST,
    PROTO_LAYOUT_CMD_MIT_TUNNING_REQUEST,
    PROTO_LAYOUT_CMD_PARAM_RELOAD_REQUEST,
    PROTO_LAYOUT_CMD_PARAM_RELOAD_RESPONSE,
    PROTO_LAYOUT_CMD_PARAM_WRITE_REQUEST,
    PROTO_LAYOUT_CMD_PARAM_WRITE_RESPONSE,
    PROTO_LAYOUT_CMD_PARAM_READ_REQUEST,
    PROTO_LAYOUT_CMD_PARAM_READ_RESPONSE,
    PROTO_LAYOUT_CMD_PARAM_SAVE_RESPONSE,
    PROTO_LAYOUT_CMD_DATA_ASK_REQUEST,
    PROTO_LAYOUT_CMD_DATA_ASK_RESPONSE,
    PROTO_LAYOUT_CMD_DATA_STREAM_SET_REQUEST,
    PROTO_LAYOUT_CMD_DATA_STREAM_SET_RESPONSE,
    PROTO_LAYOUT_CMD_CTRL_CONFIG_REQUEST,
    PROTO_LAYOUT_CMD_CTRL_CONFIG_REQUEST_OPENLOOP,
    PROTO_LAYOUT_CMD_CTRL_CONFIG_REQUEST_CURRENT,
    PROTO_LAYOUT_CMD_CTRL_CONFIG_REQUEST_PID_SPEED,
    PROTO_LAYOUT_CMD_CTRL_CONFIG_REQUEST_PID_POSITION,
    PROTO_LAYOUT_CMD_CTRL_CONFIG_REQUEST_MIT,
    PROTO_LAYOUT_CMD_OPENLOOP_CTRL_REQUEST,
    PROTO_LAYOUT_CMD_CUR_TAU_CTRL_REQUEST,
    PROTO_LAYOUT_CMD_PID_VEL_CTRL_REQUEST,
    PROTO_LAYOUT_CMD_PID_POS_CTRL_REQUEST,
    PROTO_LAYOUT_CMD_MIT_CTRL_REQUEST,
    PROTO_LAYOUT_CMD_IAP_ENTER_REQUEST,
    PROTO_LAYOUT_CMD_IAP_ENTER_RESPONSE,
    PROTO_LAYOUT_CMD_IAP_ERASE_FLASH_REQUEST,
    PROTO_LAYOUT_CMD_IAP_ERASE_FLASH_RESPONSE,
    PROTO_LAYOUT_CMD_IAP_WRITE_FLASH_REQUEST,
    PROTO_LAYOUT_CMD_IAP_WRITE_FLASH_RESPONSE,
    PROTO_LAYOUT_CMD_IAP_VERIFY_FLASH_REQUEST,
    PROTO_LAYOUT_CMD_IAP_VERIFY_FLASH_RESPONSE,
    PROTO_LAYOUT_CMD_ASK_BL_VERSION_RESPONSE,
    PROTO_LAYOUT_CMD_ASK_APP_VERSION_RESPONSE,
    PROTO_LAYOUT_CMD_ASK_DEVICE_MESSAGE_RESPONSE,
    PROTO_LAYOUT_CMD_DATA_STREAM_RESPONSE,
} eProtoLayout;

static const tProtoField g_proto_fields[] = {
    {PROTO_STR_STATE, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_STATUS, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_STATE, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_FAULT, FLD_U8, 1U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_VBUS, FLD_U16, 2U, 2U, 0U, 0U, (eProtoScale)1, 8, 0U, false},
    {PROTO_STR_DRIVER_TEMP, FLD_U8, 4U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_MOTOR_TEMP, FLD_U8, 5U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_STATUS, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_TUNE_CURRENT, FLD_F32, 0U, 4U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_TUNE_VEL, FLD_F32, 0U, 4U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_TUNE_CURRENT, FLD_F32, 0U, 4U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_TUNE_VEL, FLD_F32, 4U, 4U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_TUNE_CURRENT, FLD_F32, 0U, 4U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_TUNE_VEL, FLD_F32, 4U, 4U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_TUNE_CURRENT, FLD_F32, 0U, 4U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_TUNE_VEL, FLD_F32, 4U, 4U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_TUNE_CURRENT, FLD_F32, 0U, 4U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_TUNE_VEL, FLD_F32, 4U, 4U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_ID, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_STATUS, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_ID, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_VALUE, PROTO_TYPE_DYNAMIC, 1U, 0U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_STATUS, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_ID, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_ID, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_VALUE, PROTO_TYPE_DYNAMIC, 1U, 0U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_STATUS, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_ID, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_VALUE, PROTO_TYPE_DYNAMIC, 0U, 0U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_COUNT, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_IDS, FLD_U8, 1U, 0U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_STATUS, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_MODE, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_MODE, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_CONTROL, FLD_U8, 1U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_MODE, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_CONTROL, FLD_U8, 1U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_MODE, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_CONTROL, FLD_U8, 1U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_MODE, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_CONTROL, FLD_U8, 1U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_MODE, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_SCALE, FLD_U8, 1U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_KP, FLD_U24, 2U, 3U, 0U, 0U, (eProtoScale)3, 0, 1U, false},
    {PROTO_STR_KD, FLD_U24, 5U, 3U, 0U, 0U, (eProtoScale)3, 0, 1U, false},
    {PROTO_STR_DUTY, FLD_U16, 0U, 2U, 0U, 0U, (eProtoScale)1, 8, 0U, true},
    {PROTO_STR_VALUE, FLD_F32, 2U, 4U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_VALUE, FLD_F32, 0U, 4U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_VELOCITY, FLD_F32, 0U, 4U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_POSITION, FLD_F32, 0U, 4U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_POSITION, FLD_I24, 0U, 3U, 0U, 0U, (eProtoScale)1, 18, 0U, true},
    {PROTO_STR_VELOCITY_SCALE, FLD_I4, 3U, 1U, 0U, 3U, (eProtoScale)2, 0, 0U, false},
    {PROTO_STR_TORQUE_SCALE, FLD_I4, 3U, 1U, 4U, 7U, (eProtoScale)2, 0, 0U, false},
    {PROTO_STR_VELOCITY, FLD_I16, 4U, 2U, 0U, 0U, (eProtoScale)4, 0, 1U, true},
    {PROTO_STR_TORQUE, FLD_I16, 6U, 2U, 0U, 0U, (eProtoScale)4, 0, 2U, true},
    {PROTO_STR_VERSION, FLD_U32, 0U, 4U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_SIZE, FLD_U32, 4U, 4U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_STATUS, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_SIZE, FLD_U32, 0U, 4U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_STATUS, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_OFFSET, FLD_U32, 0U, 4U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_DATA, PROTO_TYPE_DYNAMIC, 4U, 0U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_OFFSET, FLD_U32, 0U, 4U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_STATUS, FLD_U8, 4U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_CRC32, FLD_U32, 0U, 4U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_SIZE, FLD_U32, 4U, 4U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_STATUS, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_TEXT, PROTO_TYPE_DYNAMIC, 0U, 0U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_TEXT, PROTO_TYPE_DYNAMIC, 0U, 0U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_TEXT, PROTO_TYPE_DYNAMIC, 0U, 0U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_COUNT, FLD_U8, 0U, 1U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
    {PROTO_STR_VALUES, PROTO_TYPE_DYNAMIC, 1U, 0U, 0U, 0U, (eProtoScale)0, 0, 0U, false},
};

static const tProtoLayout g_proto_layouts[] = {
    [PROTO_LAYOUT_CMD_CONNECT_REQUEST] = {0xF0, 0U, 0U, 1U, 1U, false},
    [PROTO_LAYOUT_CMD_CONNECT_RESPONSE] = {0xF0, 1U, 1U, 1U, 1U, false},
    [PROTO_LAYOUT_CMD_HEARTBEAT_RESPONSE] = {0xFB, 1U, 2U, 5U, 6U, false},
    [PROTO_LAYOUT_CMD_CORE_NRST_RESPONSE] = {0xFD, 1U, 7U, 1U, 1U, false},
    [PROTO_LAYOUT_CMD_RL_TUNNING_REQUEST] = {0xE1, 0U, 8U, 1U, 4U, false},
    [PROTO_LAYOUT_CMD_PSIF_TUNNING_REQUEST] = {0xE2, 0U, 9U, 1U, 4U, false},
    [PROTO_LAYOUT_CMD_MECH_TUNNING_REQUEST] = {0xE3, 0U, 10U, 2U, 8U, false},
    [PROTO_LAYOUT_CMD_OBS_TUNNING_REQUEST] = {0xE4, 0U, 12U, 2U, 8U, false},
    [PROTO_LAYOUT_CMD_PID_TUNNING_REQUEST] = {0xE5, 0U, 14U, 2U, 8U, false},
    [PROTO_LAYOUT_CMD_MIT_TUNNING_REQUEST] = {0xE6, 0U, 16U, 2U, 8U, false},
    [PROTO_LAYOUT_CMD_PARAM_RELOAD_REQUEST] = {0xC4, 0U, 18U, 1U, 1U, false},
    [PROTO_LAYOUT_CMD_PARAM_RELOAD_RESPONSE] = {0xC4, 1U, 19U, 1U, 1U, false},
    [PROTO_LAYOUT_CMD_PARAM_WRITE_REQUEST] = {0xC5, 0U, 20U, 2U, 1U, true},
    [PROTO_LAYOUT_CMD_PARAM_WRITE_RESPONSE] = {0xC5, 1U, 22U, 1U, 1U, false},
    [PROTO_LAYOUT_CMD_PARAM_READ_REQUEST] = {0xC6, 0U, 23U, 1U, 1U, false},
    [PROTO_LAYOUT_CMD_PARAM_READ_RESPONSE] = {0xC6, 1U, 24U, 2U, 1U, true},
    [PROTO_LAYOUT_CMD_PARAM_SAVE_RESPONSE] = {0xC7, 1U, 26U, 1U, 1U, false},
    [PROTO_LAYOUT_CMD_DATA_ASK_REQUEST] = {0xC8, 0U, 27U, 1U, 1U, false},
    [PROTO_LAYOUT_CMD_DATA_ASK_RESPONSE] = {0xC8, 1U, 28U, 1U, 0U, true},
    [PROTO_LAYOUT_CMD_DATA_STREAM_SET_REQUEST] = {0xC9, 0U, 29U, 2U, 1U, true},
    [PROTO_LAYOUT_CMD_DATA_STREAM_SET_RESPONSE] = {0xC9, 1U, 31U, 1U, 1U, false},
    [PROTO_LAYOUT_CMD_CTRL_CONFIG_REQUEST] = {0xB0, 0U, 32U, 1U, 1U, false},
    [PROTO_LAYOUT_CMD_CTRL_CONFIG_REQUEST_OPENLOOP] = {0xB0, 0U, 33U, 2U, 2U, false},
    [PROTO_LAYOUT_CMD_CTRL_CONFIG_REQUEST_CURRENT] = {0xB0, 0U, 35U, 2U, 2U, false},
    [PROTO_LAYOUT_CMD_CTRL_CONFIG_REQUEST_PID_SPEED] = {0xB0, 0U, 37U, 2U, 2U, false},
    [PROTO_LAYOUT_CMD_CTRL_CONFIG_REQUEST_PID_POSITION] = {0xB0, 0U, 39U, 2U, 2U, false},
    [PROTO_LAYOUT_CMD_CTRL_CONFIG_REQUEST_MIT] = {0xB0, 0U, 41U, 4U, 8U, false},
    [PROTO_LAYOUT_CMD_OPENLOOP_CTRL_REQUEST] = {0xB1, 0U, 45U, 2U, 6U, false},
    [PROTO_LAYOUT_CMD_CUR_TAU_CTRL_REQUEST] = {0xB3, 0U, 47U, 1U, 4U, false},
    [PROTO_LAYOUT_CMD_PID_VEL_CTRL_REQUEST] = {0xB4, 0U, 48U, 1U, 4U, false},
    [PROTO_LAYOUT_CMD_PID_POS_CTRL_REQUEST] = {0xB5, 0U, 49U, 1U, 4U, false},
    [PROTO_LAYOUT_CMD_MIT_CTRL_REQUEST] = {0xB6, 0U, 50U, 5U, 8U, false},
    [PROTO_LAYOUT_CMD_IAP_ENTER_REQUEST] = {0xA1, 0U, 55U, 2U, 8U, false},
    [PROTO_LAYOUT_CMD_IAP_ENTER_RESPONSE] = {0xA1, 1U, 57U, 1U, 1U, false},
    [PROTO_LAYOUT_CMD_IAP_ERASE_FLASH_REQUEST] = {0xA2, 0U, 58U, 1U, 4U, false},
    [PROTO_LAYOUT_CMD_IAP_ERASE_FLASH_RESPONSE] = {0xA2, 1U, 59U, 1U, 1U, false},
    [PROTO_LAYOUT_CMD_IAP_WRITE_FLASH_REQUEST] = {0xA3, 0U, 60U, 2U, 4U, true},
    [PROTO_LAYOUT_CMD_IAP_WRITE_FLASH_RESPONSE] = {0xA3, 1U, 62U, 2U, 5U, false},
    [PROTO_LAYOUT_CMD_IAP_VERIFY_FLASH_REQUEST] = {0xA4, 0U, 64U, 2U, 8U, false},
    [PROTO_LAYOUT_CMD_IAP_VERIFY_FLASH_RESPONSE] = {0xA4, 1U, 66U, 1U, 1U, false},
    [PROTO_LAYOUT_CMD_ASK_BL_VERSION_RESPONSE] = {0xA6, 1U, 67U, 1U, 0U, true},
    [PROTO_LAYOUT_CMD_ASK_APP_VERSION_RESPONSE] = {0xA7, 1U, 68U, 1U, 0U, true},
    [PROTO_LAYOUT_CMD_ASK_DEVICE_MESSAGE_RESPONSE] = {0xA8, 1U, 69U, 1U, 0U, true},
    [PROTO_LAYOUT_CMD_DATA_STREAM_RESPONSE] = {0xCA, 1U, 70U, 2U, 1U, true},
};

static const tProtoVariant g_proto_variants[] = {
    {&g_proto_layouts[PROTO_LAYOUT_CMD_CTRL_CONFIG_REQUEST], 4U, &g_proto_layouts[PROTO_LAYOUT_CMD_CTRL_CONFIG_REQUEST_OPENLOOP]},
    {&g_proto_layouts[PROTO_LAYOUT_CMD_CTRL_CONFIG_REQUEST], 5U, &g_proto_layouts[PROTO_LAYOUT_CMD_CTRL_CONFIG_REQUEST_CURRENT]},
    {&g_proto_layouts[PROTO_LAYOUT_CMD_CTRL_CONFIG_REQUEST], 6U, &g_proto_layouts[PROTO_LAYOUT_CMD_CTRL_CONFIG_REQUEST_PID_SPEED]},
    {&g_proto_layouts[PROTO_LAYOUT_CMD_CTRL_CONFIG_REQUEST], 7U, &g_proto_layouts[PROTO_LAYOUT_CMD_CTRL_CONFIG_REQUEST_PID_POSITION]},
    {&g_proto_layouts[PROTO_LAYOUT_CMD_CTRL_CONFIG_REQUEST], 8U, &g_proto_layouts[PROTO_LAYOUT_CMD_CTRL_CONFIG_REQUEST_MIT]},
};

#define PROTO_LAYOUT_COUNT 44U
#define PROTO_VARIANT_COUNT 5U

#endif
