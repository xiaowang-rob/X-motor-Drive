#ifndef __SENSE_H
#define __SENSE_H

#include "device.h"

// ============================================================
// sense.h — 电流/电压/温度采样业务对象（abs）
//
// 接口形态：ops + handle（装配层在定义 tSense 对象时挂上）。
//
// 吃原始码 → 输出物理量：
//   - 三相电流：原始码 - 零点（EMA 跟踪）× 换算系数
//   - Vbus：原始码 × 换算系数
//   - 温度：NTC 采样码经 Vbus 归一后查表
//
// 典型调用：PWM 中断里每周期 sense_update(s, 电机是否空闲)；
// 空闲阶段自动累积零点（校准），运行阶段输出电流。
// ============================================================

#define SENSE_IDLE_K 0.002f     // 零点 EMA 系数
#define SENSE_VT_REFRESH_MS 10U // Vbus/温度刷新周期

// 驱动接口（板级实现）；handle 为驱动实例
typedef struct
{
    bool (*open)(void *handle, float *cur_scale, float *vbus_scale); // 启动采样前端 + 给换算系数
    void (*set_sample_point)(void *handle, uint32_t tic);            // 电流采样点在 PWM 周期内的位置
    bool (*get_cur_raw)(void *handle, uint16_t raw[3]);              // 取最新一帧三相电流原始码
    void (*vt_trigger)(void *handle);                                // 触发一轮 Vbus/温度转换
    bool (*get_vt_raw)(void *handle, uint16_t *vbus_raw,
                       uint16_t *temp_raw);                          // 取 Vbus/温度原始码
} tSenseOps;

typedef struct
{
    const tSenseOps *ops; // 驱动 ops（装配时挂）
    void *handle;         // 驱动实例（装配时挂）

    // 换算系数（init 时由驱动给出，之后不变）
    float cur_scale;  // A/码（12bit 电流）
    float vbus_scale; // V/码（8bit Vbus）

    // 电流零点（码域，EMA 跟踪）
    float cur_zero[3];
    bool zero_ready;

    // 结果缓存（供 FOC 中断读取）
    float cur[3];      // A
    float vbus;        // V
    float temperature; // ℃

    uint32_t last_vt_ms; // 上次 Vbus/温度刷新时刻
} tSense;

// 启动对象：ops/handle 须已由装配层挂好（任一为空返回 false）
bool sense_init(tSense *s);

// 设置电流采样点在 PWM 周期内的位置（转发给驱动）
void sense_set_sample_point(tSense *s, uint32_t tic);

// PWM 周期内调用一次：
//   motor_idle=true  → 用当前码更新零点 EMA，电流输出 0（校准阶段）
//   motor_idle=false → 输出换算电流
//   内部按 SENSE_VT_REFRESH_MS 节流刷新 Vbus/温度
void sense_update(tSense *s, bool motor_idle);

// ---- 查询（多在中断上下文） ----
void sense_get_current(const tSense *s, float *iu, float *iv, float *iw);
float sense_get_vbus(const tSense *s);
float sense_get_temperature(const tSense *s);
bool sense_is_zero_ready(const tSense *s);

#endif // __SENSE_H
