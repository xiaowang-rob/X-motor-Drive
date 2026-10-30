#ifndef __PWM_H
#define __PWM_H

#include "device.h"

// ============================================================
// gate_drv.h — 栅极驱动 / 电机功率级业务对象（abs）
//
// 接口形态：类型化 DevHandle（ops + dev），由 init 显式注入。
//
// 热路径提示：gate_drv_set_compare 由 FOC 下溢中断以 20kHz 调用，
// 调用链为 ctl → gate_drv_set_compare → ops->set_compare（一层间接调用）。
// 若需零开销，对 abs 与 drv 同时开启 LTO 即可整链内联。
// ============================================================

// 驱动接口（板级实现）；handle 为驱动实例
typedef struct {
    bool (*open)(void *handle, uint32_t *pwm_period); // 取回 PWM 周期计数（ARR）
    void (*power)(void *handle, bool on);             // 12V 功率级电源开关
    void (*enable)(void *handle, bool en);            // PWM 输出使能 / 关断
    void (*set_compare)(void *handle, uint16_t a, uint16_t b,
                        uint16_t c); // 三相占空比（热路径）
} tGateOps;

// BSP 导出的类型化句柄：操作表 + 驱动实例。
typedef struct {
    const tGateOps *ops;
    void *dev;
} tGateDevHandle;

typedef struct {
    const tGateDevHandle *handle; // 驱动句柄，由 init 注入

    uint32_t pwm_period;  // PWM 周期计数（init 时自驱动取回）
    eDeviceStatus dstate; // 设备状态
} tGateDrv;

// 启动对象：显式传入 BSP 导出的类型化句柄；缓冲由调用方提供。
bool gate_drv_init(tGateDrv *drv, const tGateDevHandle *handle);

void gate_drv_power_on(tGateDrv *drv, bool on);
void gate_drv_enable(tGateDrv *drv, bool en);
void gate_drv_set_compare(tGateDrv *drv, uint16_t ticA, uint16_t ticB, uint16_t ticC);

static inline uint32_t gate_drv_get_pwm_period(const tGateDrv *drv) { return drv->pwm_period; }
static inline eDeviceStatus gate_drv_get_status(const tGateDrv *drv) { return drv->dstate; }

#endif // __PWM_H
