#ifndef __ENC_H
#define __ENC_H

#include "device_status.h"

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

typedef struct ABI_CONFIG {
    uint32_t enc_counts_per_rev; // A/B 编码器每机械圈计数 线数
    float filter;                // 0-1
    bool index_falling;
} tAbiCfg;

typedef struct HALL_ENCODER_CONFIG {
    uint8_t
        sequence[8]; // 电角度正向 Hall 状态序列，必须为1..6的合法单比特相邻环
    uint16_t prescaler; // 捕获时基 PSC，不是 PWM 分频
    float filter;       // 滤波 0-1
} tHallEncCfg;

#define SC_MIN_AMPLITUDE 100 // 校正后最小向量幅值
typedef struct SINCOS_ENCODER_CONFIG {
    uint32_t frequency_hz;
    float sin_offset, cos_offset; // 原始12位码域
    float sin_gain, cos_gain;     // 校正后的幅值比例，必须为正
} tSinCosEncCfg;

typedef enum {
    BSP_ANGLE_MECHANICAL, // 机械角度
    BSP_ANGLE_ELECTRICAL, // 电角度
} eBspAngleKind;

typedef struct {
    uint16_t raw; // 保留 SPI 码值；其他模式为计数/Hall状态
    uint32_t ts_us;
    float angle_rad; // [0,2pi)，含义由 angle_kind 指明
    eBspAngleKind angle_kind;
    bool referenced; // ABI 为是否见过 Z；是否能表示绝对位置
    uint16_t sin_raw, cos_raw;
    uint32_t interval_ticks; // Hall 相邻跳变计时
    int8_t direction;        // Hall 相序方向，-1/0/+1
} tEncSample;

// 驱动接口（板级实现）；dev 为驱动实例
typedef struct {
    bool (*init)(void *dev, void *cfg);          // 初始化
    bool (*open)(void *dev);                     // 启动该路
    bool (*close)(void *dev);                    // 停止该路
    bool (*read)(void *dev, tEncSample *sample); // 读一次采样值
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
bool enc_init(tEncoder *enc, const tEncDevHandle *handle);

// 周期任务：读取一次角度并更新状态
void enc_task(tEncoder *enc);

#endif // __ENC_H
