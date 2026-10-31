#ifndef __ENCODER_H
#define __ENCODER_H

#include "device_status.h"
#include "parameter_list.h"

// ============================================================
// encoder.h — 编码器业务对象（abs）
//
// 接口形态：ops + handle
//   驱动（board/<B>/drv/enc_*.c）直接引用本头实现 tEncoderOps，并导出
//   const ops 与驱动实例符号；装配层（board/<B>/bsp/bsp_cfg.c）在定义
//   tEncoder 对象时就地挂上 ops/handle。
//   abs 只透传芯片选型参数，不解析芯片协议，也不感知外设与引脚。
// ============================================================

#define ENCODER_ERR_VALID_LIMIT 100 // valid_counter 超过该值判编码器通讯出问题
#define ENCODER_VALID_COUNT_MAX 110 // 失效方向的饱和值

typedef enum {
    BSP_ANGLE_MECHANICAL,  // 机械角度
    BSP_ANGLE_ELECTRICAL,  // 电角度
    BSP_ANGLE_RELATIVE,    // 相对角度
    BSP_ANGLE_SENSOR_PHASE // 相位
} eBspAngleKind;

typedef struct {
    uint16_t raw;        // 保留 SPI 码值；其他模式为计数/Hall状态
    uint32_t resolution; // 支持 ABI 65536
    uint32_t ts_ms;
    float angle_rad; // [0,2pi)，含义由 angle_kind 指明
    eBspAngleKind angle_kind;
    bool referenced; // ABI 为是否见过 Z；Hall 不表示机械零点
    uint16_t sin_raw, cos_raw;
    uint32_t interval_ticks; // Hall 相邻跳变计时
    int8_t direction;        // Hall 相序方向，-1/0/+1
} tEncSample;

// 驱动接口（板级实现）；handle 为驱动实例
typedef struct {
    bool (*open)(void *handle);                     // 启动该路
    bool (*close)(void *handle);                    // 停止该路
    bool (*read)(void *handle, tEncSample *sample); // 读一次采样值
    void (*abort)(void *handle);                    // 中止（抬 CS 等）
} tEncoderOps;

typedef struct {
    const tEncoderOps *ops; // 驱动 ops（装配时挂）
    void *dev;              // 驱动实例（装配时挂）
} tEncDevHandle;

typedef struct {
    const tEncDevHandle *handle; // 驱动 handle（装配时挂）

    eDeviceStatus dstate; // 设备状态

    tEncSample sample; // 本次采样值

    uint16_t valid_counter; // 数据有效性滑动指示（成功 -1 / 失败 +10）
} tEncoder;

extern tEncoder g_enc;

// 启动对象：ops/handle 须已由装配层挂好（任一为空返回 false）
bool encoder_init(tEncoder *enc, const tEncDevHandle *handle);

// 周期任务：读取一次角度并更新状态
void encoder_task(tEncoder *enc);

// ---- 查询 ----

static inline eDeviceStatus encoder_get_dev_state(const tEncoder *enc) {
    return enc->dstate;
}

#endif // __ENCODER_H
