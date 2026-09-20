#ifndef __ENCODER_H
#define __ENCODER_H

#include "device.h"

// ============================================================
// encoder.h — 编码器业务对象（abs）
//
// 接口形态：ops + handle
//   驱动（board/<B>/drv/enc_*.c）直接引用本头实现 tEncoderOps，并导出
//   const ops 与驱动实例符号；装配层（board/<B>/bsp/bsp_cfg.c）在定义
//   tEncoder 对象时就地挂上 ops/handle。
//   abs 只认 ops 与不透明 handle，不感知型号、外设与引脚。
// ============================================================

typedef enum
{
    ENC_OFF,  // 编码器未开启
    ENC_MAIN, // 主编码器 主要的角度获取
    ENC_AUX,  // 辅编码器 启动时的辅助角度获取
} eEncoderMode;

#define ENCODER_ERR_VALID_LIMIT 100 // valid_counter 超过该值判编码器通讯出问题
#define ENCODER_VALID_COUNT_MAX 110 // 失效方向的饱和值

// 驱动接口（板级实现）；handle 为驱动实例
typedef struct
{
    bool (*open)(void *handle, uint16_t *resolution);           // 启动该路并给出单圈分辨率
    bool (*read)(void *handle, uint16_t *raw, uint32_t *ts_ms); // 读一次原始角 + 时间戳
    void (*abort)(void *handle);                                // 中止（抬 CS 等）
} tEncoderOps;

typedef struct
{
    const tEncoderOps *ops; // 驱动 ops（装配时挂）
    void *handle;           // 驱动实例（装配时挂）

    eEncoderMode mode; // 编码器模式（主/辅）

    eDeviceStatus dstate;   // 设备状态
    uint16_t resolution;    // 单圈分辨率（open 时由驱动给出，之后不变）
    float rad_per_lsb;      // 每 LSB 弧度（由 resolution 派生）
    float angle_abs;        // 本次绝对角度 [0, 2π)
    uint16_t valid_counter; // 数据有效性滑动指示（成功 -1 / 失败 +10）
} tEncoder;

// 启动对象：ops/handle 须已由装配层挂好（任一为空返回 false）
bool encoder_init(tEncoder *enc, eEncoderMode mode);

// 周期任务：读取一次角度并更新状态
void encoder_task(tEncoder *enc);

// ---- 查询 ----
static inline float encoder_get_angle_abs(const tEncoder *enc) { return enc->angle_abs; }
static inline eDeviceStatus encoder_get_dev_state(const tEncoder *enc) { return enc->dstate; }
static inline uint16_t encoder_get_resolution(const tEncoder *enc) { return enc->resolution; }

#endif // __ENCODER_H
