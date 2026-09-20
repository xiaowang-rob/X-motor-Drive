#ifndef __GATE_DRV_H
#define __GATE_DRV_H

#include "device.h"

// ============================================================
// gate_drv.h — 栅极驱动 / 电机功率级业务对象（abs）
//
// 接口形态：ops + handle（装配层在定义 tGateDrv 时就地挂上）。
//
// 热路径提示：gate_drv_set_compare 由 FOC 下溢中断以 20kHz 调用，
// 调用链为 ctl → gate_drv_set_compare → ops->set_compare（一层间接调用）。
// 若需零开销，对 abs 与 drv 同时开启 LTO 即可整链内联。
// ============================================================

// 驱动接口（板级实现）；handle 为驱动实例
typedef struct
{
    bool (*open)(void *handle, uint32_t *pwm_period); // 取回 PWM 周期计数（ARR）
    void (*power)(void *handle, bool on);             // 12V 功率级电源开关
    void (*enable)(void *handle, bool en);            // PWM 输出使能 / 关断
    void (*set_compare)(void *handle, uint16_t a, uint16_t b,
                        uint16_t c); // 三相占空比（热路径）
    void (*set_isr)(void *handle, void (*sample_cb)(void),
                    void (*ctrl_cb)(void)); // 注册上溢/下溢处理函数
} tGateOps;

typedef struct
{
    const tGateOps *ops; // 驱动 ops（装配时挂）
    void *handle;        // 驱动实例（装配时挂）

    uint32_t pwm_period;  // PWM 周期计数（init 时自驱动取回）
    eDeviceStatus dstate; // 设备状态
} tGateDrv;

// 启动对象：ops/handle 须已由装配层挂好（任一为空返回 false）
bool gate_drv_init(tGateDrv *drv);

void gate_drv_power_on(tGateDrv *drv, bool on);
void gate_drv_enable(tGateDrv *drv, bool en);
void gate_drv_set_compare(tGateDrv *drv, uint16_t ticA, uint16_t ticB, uint16_t ticC);

// FOC 节拍回调注册（转发给驱动；TIM 中断由板级集中分发持有）
//   sample_cb —— 下溢（2-shunt 电流采样点）
//   ctrl_cb   —— 上溢（FOC 控制主循环）
void gate_drv_register_isrs(tGateDrv *drv, void (*sample_cb)(void), void (*ctrl_cb)(void));

static inline uint32_t gate_drv_get_pwm_period(const tGateDrv *drv) { return drv->pwm_period; }
static inline eDeviceStatus gate_drv_get_status(const tGateDrv *drv) { return drv->dstate; }

#endif // __GATE_DRV_H
