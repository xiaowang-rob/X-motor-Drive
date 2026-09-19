#ifndef XDR_APP_ABS_ENCODER_H
#define XDR_APP_ABS_ENCODER_H

#include "device.h"

// ============================================================
// encoder.h — 编码器业务对象（abs）
//
// 编译期绑定：本层**不持有 ops 表、不做 void* 强转**。初始化与读角
// 经由板级钩子（见 encoder_board.h，由 board/<B>/drv 实现一份），
// 由链接器在编译期解析——热路径上是一次真实的函数调用，不是间接跳转。
//
// 本层只做"原始角读数 → 业务量"的加工，不感知芯片型号、SPI、引脚。
//
//   职责边界：
//     abs  角度换算 / 数据有效性（valid_counter）/ 设备状态
//     drv  芯片协议、SPI 时序、CS 管理，只返回单次操作成败
// ============================================================

typedef enum
{
    EXT_ENCODER = 0, // 外部编码器
    INT_ENCODER = 1, // 内置编码器
} eEncoderType;

// 编码器芯片型号。
// 数值须与协议层 eEncoderChip（app/app/protocol.h）逐一对齐，
// 由 device_cfg.c 的 _Static_assert 在编译期校验。
typedef enum
{
    ENC_CHIP_NONE = 0,
    ENC_CHIP_MT6816 = 1,
    ENC_CHIP_MT6835 = 2,
    ENC_CHIP_AS5047 = 3,
} eEncoderChipId;

#define ENCODER_ERR_VALID_LIMIT 100 // valid_counter 超过该值判编码器通讯出问题
#define ENCODER_VALID_COUNT_MAX 110 // 失效方向的饱和值

typedef struct
{
    eEncoderType type;      // 编码器类型（内/外）
    eDeviceStatus dstate;   // 设备状态
    uint16_t resolution;    // 单圈分辨率（open 时由板级给出，之后不变）
    float rad_per_lsb;      // 每 LSB 弧度（由 resolution 派生）
    float angle_abs;        // 本次绝对角度 [0, 2π)
    uint16_t valid_counter; // 数据有效性滑动指示（成功 -1 / 失败 +10）
} tEncoder;

// 绑定并初始化（内部调用板级钩子 encoder_board_open）
bool encoder_init(tEncoder *enc, eEncoderType type, eEncoderChipId chip);

// 周期任务：读取一次角度并更新状态
void encoder_task(tEncoder *enc);

// ---- 查询 ----
static inline float encoder_get_angle_abs(const tEncoder *enc) { return enc->angle_abs; }
static inline eDeviceStatus encoder_get_dev_state(const tEncoder *enc) { return enc->dstate; }
static inline uint16_t encoder_get_resolution(const tEncoder *enc) { return enc->resolution; }

#endif // XDR_APP_ABS_ENCODER_H
