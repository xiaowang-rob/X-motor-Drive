#ifndef __ABS_ENCODER_H
#define __ABS_ENCODER_H

#include "device.h"

// ============================================================
// encoder.h — 编码器（usr/abs）
//
// 分两部分：
//   [驱动契约]  drv 与业务之间的接口形状（句柄 + 语义化 ops 表）
//   [业务对象]  tEncoder：把"原始角度读数"加工成电机控制所需量：
//               多圈累计 / 零位 / 位置 / M-T 测速 / PLL 平滑 / 数据有效性
//
// 规则：
//   - 业务对象只依赖 ops 契约，不感知具体芯片与板级资源
//   - 角度单位 rad；多圈位置连续增长（可负）
//   - 时间戳由驱动在读数时打上（abs 不直接取时钟）
// ============================================================

// ==================== 驱动契约 ====================

typedef void *EncoderChipHandle;

typedef enum
{
    EXT_ENCODER,
    INT_ENCODER,
} eEncoderType;
typedef struct
{
    // 芯片初始化（含按芯片协议配置 SPI 模式）
    bool (*init)(EncoderChipHandle h, eEncoderType type);

    // 读取一次最新角度（同步）：成功输出 raw（0~分辨率-1）与时间戳(ms)
    bool (*read_angle)(EncoderChipHandle h, uint16_t *raw, uint32_t *ts_ms);

    // 单圈分辨率，如 16384
    bool (*get_resolution)(EncoderChipHandle h, uint16_t *res);

    // 复位芯片
    void (*reset)(EncoderChipHandle h);

} tEncoderDriverOps;

// ==================== 业务对象 ====================

#define ENCODER_ERR_VALID_LIMIT 100 // valid_counter 超过该值判编码器通讯出问题

typedef struct
{
    const tEncoderDriverOps *drv_ops; // 绑定的芯片驱动 ops
    EncoderChipHandle drv_handle;     // 芯片句柄
    eEncoderType type;                // 编码器类型（外部/内部）
    eDeviceStatus dstate;             // 设备状态
    uint16_t resolution;              // 单圈分辨率
    float rad_per_lsb;                // 每 LSB 弧度

    // ---- 业务状态 ----
    float angle_abs; // 本次绝对角度 [0, 2π)

    // ---- 数据有效性 ----
    uint16_t valid_counter; // 有效数据计数（成功-1/失败+10 的滑动指示）
} tEncoder;

// 绑定驱动并初始化（含调用 ops->init）
bool encoder_init(tEncoder *enc, const tEncoderDriverOps *ops,
                  EncoderChipHandle handle, eEncoderType type);

// 处理任务：读取一次角度并更新
void encoder_task(tEncoder *enc);

// ---- 查询 ----
static inline float encoder_get_angle_abs(tEncoder *enc) { return enc->angle_abs; }
static inline eDeviceStatus encoder_get_dev_state(tEncoder *enc) { return enc->dstate; }

#endif // __ABS_ENCODER_H
