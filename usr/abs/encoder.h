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

// 默认 PLL 增益与错误判定
#define ENCODER_PLL_KP 80.0f
#define ENCODER_PLL_KI 2000.0f
#define ENCODER_PLL_INTEG_LIMIT 0.1745f // ±10°
#define ENCODER_VEL_PHYS_LIMIT 1046.0f  // rad/s 物理上限（≈10k rpm）
#define ENCODER_ERR_VALID_LIMIT 100     // valid_counter 超过该值判编码器通讯出问题

typedef struct
{
    const tEncoderDriverOps *drv_ops; // 绑定的芯片驱动 ops
    EncoderChipHandle drv_handle;     // 芯片句柄
    eEncoderType type;                // 编码器类型（外部/内部）
    eDeviceStatus dstate;             // 设备状态
    uint16_t resolution;              // 单圈分辨率
    float rad_per_lsb;                // 每 LSB 弧度

    // ---- 业务状态 ----
    float angle_abs;   // 本次绝对角度 [0, 2π)
    float pos;         // 多圈连续位置（rad）
    float vel;         // M/T 测速（rad/s，未平滑）
    int32_t num_turns; // 累计转数

    // ---- 内部变量 ----
    uint16_t last_raw_angle; // 上一次原始角度
    float pos_offset;        // 零位偏移（原始角度计数）
    uint32_t last_ts_ms;     // 上一次时间戳
    float last_angle_abs;    // 上一次绝对角度

    // ---- PLL（角度/速度平滑） ----
    float pll_theta;       // PLL 输出角度
    float pll_vel;         // PLL 输出速度（rad/s）
    float pll_integ;       // 积分累加
    float pll_theta_delta; // 误差

    // ---- 数据有效性 ----
    uint16_t valid_counter; // 有效数据计数（成功-1/失败+10 的滑动指示）
    bool first_run;         // 首次读数标志
} tEncoder;

// 绑定驱动并初始化（含调用 ops->init）
bool encoder_init(tEncoder *enc, const tEncoderDriverOps *ops,
                  EncoderChipHandle handle, eEncoderType type);

// 处理任务：读取一次角度并更新多圈位置/测速/有效性
void encoder_task(tEncoder *enc);

// PLL 平滑更新（由上层按固定周期 dt 调用，输出 pll_vel/pll_theta）
void encoder_pll_update(tEncoder *enc, float dt);

// 以当前角度为零位
void encoder_set_zero(tEncoder *enc);

// ---- 查询 ----
static inline float encoder_get_angle_abs(tEncoder *enc) { return enc->angle_abs; }
static inline float encoder_get_position(tEncoder *enc) { return enc->pos; }
static inline float encoder_get_velocity(tEncoder *enc) { return enc->vel; }
static inline float encoder_get_pll_velocity(tEncoder *enc) { return enc->pll_vel; }
static inline float encoder_get_pll_angle(tEncoder *enc) { return enc->pll_theta; }
static inline int32_t encoder_get_turns(tEncoder *enc) { return enc->num_turns; }
static inline eDeviceStatus encoder_get_dev_state(tEncoder *enc) { return enc->dstate; }

#endif // __ABS_ENCODER_H
