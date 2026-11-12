#ifndef __SAMPLE_H
#define __SAMPLE_H

#include "device_status.h"

// ============================================================
// sample.h — 电流/电压/温度采样业务对象（abs）
//
// 接口形态：类型化 DevHandle（ops + dev），由 init 显式注入。
//
// 吃原始码 → 输出物理量：
//   - 三相电流：原始码 - 零点（EMA 跟踪）× 换算系数
//   - Vbus：原始码 × 换算系数
//   - 温度：NTC 采样码经 Vbus 归一后查表
//
// 典型调用：PWM 中断里每周期 sample_update(s, 电机是否空闲)；
// 空闲阶段自动累积零点（校准），运行阶段输出电流。
// ============================================================

#define SENSE_IDLE_K 0.002f // 零点 EMA 系数

// 驱动接口（板级实现）；handle 为驱动实例
typedef struct {
    bool (*open)(void *handle); // 启动采样前端
    void (*set_sample_point)(void *handle,
                             uint32_t tic); // 电流采样点在 PWM 周期内的位置
    bool (*get_cur_raw)(void *handle,
                        float raw[3]); // 取最新一帧三相电流(未零点处理)
    bool (*get_vt)(void *handle, float *vbus, uint8_t *temp); // 取 Vbus/温度
} tSampleOps;

// BSP 导出的类型化句柄：操作表 + 驱动实例。
typedef struct {
    const tSampleOps *ops;
    void *dev;
} tSampleDevHandle;

typedef struct {
    const tSampleDevHandle *handle; // 驱动句柄，由 init 注入
    eDeviceStatus dstate;           // 设备状态
    // 电流零点（码域，EMA 跟踪）
    float cur_zero[3];
    bool zero_ready;

    // 结果缓存（供 FOC 中断读取）
    float cur[3];        // A
    float vbus;          // V
    uint8_t temperature; // ℃

} tSample;

extern tSample g_sample;

// 启动对象：显式传入 BSP 导出的类型化句柄；缓冲由调用方提供。
bool sample_init(tSample *s, const tSampleDevHandle *handle);

// 设置电流采样点在 PWM 周期内的位置（转发给驱动）
void sample_set_sample_point(tSample *s, uint32_t tic);
// 更新 空闲时EMA电流零点跟随 使能时停止跟随
void sample_update(tSample *s, bool motor_idle);

// ---- 查询 ----
static inline void sample_get_current(const tSample *s, float is[3]) {
    if (!s)
        return;
    is[0] = s->cur[0];
    is[1] = s->cur[1];
    is[2] = s->cur[2];
}
static inline float sample_get_vbus(const tSample *s) {
    return s ? s->vbus : 0.0f;
}
static inline float sample_get_temperature(const tSample *s) {
    return s ? s->temperature : 0.0f;
}

#endif // __SAMPLE_H
