#ifndef __ABS_SENSE_H
#define __ABS_SENSE_H

#include "device.h"

// ============================================================
// sense.h — 采样契约与业务对象（abs）
//
// 本头同时定义 abs↔drv 的采样接口契约 tSampleMcuOps：
// 描述"由定时器触发、DMA 落缓冲的 ADC 采样前端"最小能力，
// 实现 = 板级采样驱动（直连 HAL）。业务对象吃原始码输出物理量。
//
// 约定：ops 表内不存 ctx / 外设；每个函数首参为该实例的 SampleHandle。
// ============================================================

typedef void *SampleHandle; // 采样前端实例句柄（实体由驱动定义）

// ---- abs↔drv 采样接口契约 ----
typedef struct
{
    // 初始化采样前端（启动 DMA 与首轮 Vbus/温度转换）；返回前资源应可用
    bool (*init)(SampleHandle h);

    // 设置电流采样点在 PWM 周期内的位置（定时器比较值，计数单位）
    void (*set_sample_cmp)(SampleHandle h, uint32_t tic);

    // 取最新一帧三相电流原始码（12bit，0~4095）
    bool (*get_cur_raw)(SampleHandle h, uint16_t raw[3]);

    // 触发一轮 Vbus/温度转换（软件触发；结果经 get_vt_raw 取回）
    void (*vt_trigger)(SampleHandle h);

    // 取最新 Vbus/温度原始码；返回 false 表示自上次读取后无新帧
    bool (*get_vt_raw)(SampleHandle h, uint16_t *vbus_raw, uint16_t *temp_raw);

    // 线性换算系数：码 → 物理量（cur_scale=A/码12bit、vbus_scale=V/码8bit）
    void (*get_gain)(SampleHandle h, float *cur_scale, float *vbus_scale);
} tSampleMcuOps;

// ============================================================
// sense.h — 电流/电压/温度采样业务对象（abs，纯逻辑）
//
// 吃 tSampleMcuOps（原始码）→ 输出物理量：
//   - 三相电流：原始码 - 零点（EMA 跟踪）× 换算系数
//   - Vbus：原始码 × 换算系数
//   - 温度：NTC 采样码经 Vbus 归一后查表
//
// 典型调用：PWM 中断里每周期 sense_update(s, 电机是否空闲)；
// 空闲阶段自动累积零点（校准），运行阶段输出电流。
// ============================================================

#define SENSE_IDLE_K 0.002f     // 零点 EMA 系数
#define SENSE_VT_REFRESH_MS 10U // Vbus/温度刷新周期

typedef struct
{
    const tSampleMcuOps *ops; // 注入：板采样驱动 ops
    SampleHandle handle;      // 采样前端实例句柄

    // 换算系数（init 时由 ops->get_gain 取）
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

bool sense_init(tSense *s, const tSampleMcuOps *ops, SampleHandle handle);

// 设置电流采样点在 PWM 周期内的位置（转发给采样前端）
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

#endif // __ABS_SENSE_H
