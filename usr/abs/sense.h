#ifndef __ABS_SENSE_H
#define __ABS_SENSE_H

#include <stdint.h>
#include <stdbool.h>

#include "usr/abs/time.h"

// ============================================================
// sense.h — 采样业务对象（usr/abs）
//
// 本头同时定义 abs↔drv 的采样接口契约 tSampleIf（v2 收编自 usr/if）：
// 描述"由定时器触发、DMA 落缓冲的 ADC 采样前端"最小能力，
// 实现 = usr/drv 的板采样驱动（直调 HAL）。业务对象吃 tSampleIf 输出物理量。
// ============================================================

// ---- abs↔drv 采样接口契约 ----
typedef struct
{
    void *ctx; // 实现侧资源（对调用方不透明）

    // 初始化采样前端（启动 DMA 与首轮 Vbus/温度转换）；返回前资源应可用
    bool (*init)(void *ctx);

    // 设置电流采样点在 PWM 周期内的位置（定时器比较值，计数单位）
    void (*set_sample_cmp)(void *ctx, uint32_t tic);

    // 取最新一帧三相电流原始码（12bit，0~4095）
    bool (*get_cur_raw)(void *ctx, uint16_t raw[3]);

    // 触发一轮 Vbus/温度转换（软件触发；结果经 get_vt_raw 取回）
    void (*vt_trigger)(void *ctx);

    // 取最新 Vbus/温度原始码；返回 false 表示自上次读取后无新帧
    bool (*get_vt_raw)(void *ctx, uint16_t *vbus_raw, uint16_t *temp_raw);

    // 线性换算系数：码 → 物理量（cur_scale=A/码12bit、vbus_scale=V/码8bit）
    void (*get_gain)(void *ctx, float *cur_scale, float *vbus_scale);
} tSampleIf;

// ============================================================
// sense.h — 电流/电压/温度采样业务对象（usr/abs，纯逻辑）
//
// 吃 tSampleIf（原始码）→ 输出物理量：
//   - 三相电流：原始码 - 零点（EMA 跟踪）× 换算系数
//   - Vbus：原始码 × 换算系数
//   - 温度：NTC 采样码经 Vbus 归一后查表
//
// 典型调用：PWM 中断里每周期 sense_update(s, 电机是否空闲)；
// 空闲阶段自动累积零点（校准），运行阶段输出电流。
// ============================================================

#define SENSE_IDLE_K 0.002f       // 零点 EMA 系数
#define SENSE_VT_REFRESH_MS 10U   // Vbus/温度刷新周期

typedef struct
{
    const tSampleIf *sample; // 注入：板采样驱动（usr/drv，直调 HAL）
    const tTimeIf *time;     // 注入：时间基准（Vbus/温度节流）

    // 换算系数（init 时由 sample->get_gain 取）
    float cur_scale;  // A/码（12bit 电流）
    float vbus_scale; // V/码（8bit Vbus）

    // 电流零点（码域，EMA 跟踪）
    float cur_zero[3];
    bool zero_ready;

    // 结果缓存（供 FOC 中断读取）
    float cur[3];     // A
    float vbus;       // V
    float temperature; // ℃

    uint32_t last_vt_ms; // 上次 Vbus/温度刷新时刻
} tCurrentSense;

bool sense_init(tCurrentSense *s, const tSampleIf *sample, const tTimeIf *time);

// 设置电流采样点在 PWM 周期内的位置（转发给采样前端）
void sense_set_sample_point(tCurrentSense *s, uint32_t tic);

// PWM 周期内调用一次：
//   motor_idle=true  → 用当前码更新零点 EMA，电流输出 0（校准阶段）
//   motor_idle=false → 输出换算电流
//   内部按 SENSE_VT_REFRESH_MS 节流刷新 Vbus/温度
void sense_update(tCurrentSense *s, bool motor_idle);

// ---- 查询（多在中断上下文） ----
void sense_get_current(const tCurrentSense *s, float *iu, float *iv, float *iw);
float sense_get_vbus(const tCurrentSense *s);
float sense_get_temperature(const tCurrentSense *s);
bool sense_is_zero_ready(const tCurrentSense *s);

#endif // __ABS_SENSE_H
