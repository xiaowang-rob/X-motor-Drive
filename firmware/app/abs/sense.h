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
    bool (*open)(void *handle);                               // 启动采样前端
    void (*set_sample_point)(void *handle, uint32_t tic);     // 电流采样点在 PWM 周期内的位置
    bool (*get_cur_raw)(void *handle, float raw[3]);          // 取最新一帧三相电流(未零点处理)
    bool (*get_vt)(void *handle, float *vbus, uint8_t *temp); // 取 Vbus/温度
} tSenseOps;

typedef struct
{
    const tSenseOps *ops; // 驱动 ops（装配时挂）
    void *handle;         // 驱动实例（装配时挂）
    eDeviceStatus dstate; // 设备状态
    // 电流零点（码域，EMA 跟踪）
    float cur_zero[3];
    bool zero_ready;

    // 结果缓存（供 FOC 中断读取）
    float cur[3];        // A
    float vbus;          // V
    uint8_t temperature; // ℃

} tSense;

// 启动对象：ops/handle 须已由装配层挂好（任一为空返回 false）
bool sense_init(tSense *s);

// 设置电流采样点在 PWM 周期内的位置（转发给驱动）
void sense_set_sample_point(tSense *s, uint32_t tic);
// 更新 空闲时EMA电流零点跟随 使能时停止跟随
void sense_update(tSense *s, bool motor_idle);

// ---- 查询 ----
static inline void sense_get_current(const tSense *s, float *iu, float *iv, float *iw)
{
    if (!s)
        return;
    *iu = s->cur[0];
    *iv = s->cur[1];
    *iw = s->cur[2];
}
static inline float sense_get_vbus(const tSense *s)
{
    return s ? s->vbus : 0.0f;
}
static inline float sense_get_temperature(const tSense *s)
{
    return s ? s->temperature : 0.0f;
}

#endif // __SENSE_H
