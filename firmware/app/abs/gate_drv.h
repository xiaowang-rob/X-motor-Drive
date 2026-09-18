#ifndef __GATE_DRV_H
#define __GATE_DRV_H

#include "device.h"

// ============================================================
// gate_drv.h — 栅极驱动 / 电机功率级契约（abs）
//
// 驱动侧（drv）以"不透明句柄"实现本 ops：句柄实体由驱动定义，
// 内含该实例的 ops 指针、外设句柄与配置（见 gate_fd6288q.c）。
// 业务对象 tGateDrv 只持有 ops + handle，不感知定时器/引脚。
// ============================================================

typedef void *GateHandle;

typedef struct
{
    // 获取 PWM 配置（定时器周期计数值）
    void (*get_pwm_config)(GateHandle h, uint32_t *pwm_period);
    // 功率级电源开关
    void (*power_ctrl)(GateHandle h, bool on);
    // PWM 输出使能 / 关断
    void (*start)(GateHandle h);
    void (*stop)(GateHandle h);
    // 三相占空比（比较值）
    void (*set_compare)(GateHandle h, uint16_t ticA, uint16_t ticB, uint16_t ticC);
} tGateDrvOps;

typedef struct
{
    const tGateDrvOps *ops; // 绑定的驱动 ops
    GateHandle handle;      // 驱动实例句柄
    uint32_t pwm_period;    // PWM 周期计数（init 时自驱动取回）
    eDeviceStatus dstate;   // 设备状态
} tGateDrv;

bool gate_drv_init(tGateDrv *drv, const tGateDrvOps *ops, GateHandle handle);
void gate_drv_power_on(tGateDrv *drv, bool on);
void gate_drv_enable(tGateDrv *drv, bool en);
void gate_drv_set_compare(tGateDrv *drv, uint16_t ticA, uint16_t ticB, uint16_t ticC);

static inline uint32_t gate_drv_get_pwm_period(const tGateDrv *drv) { return drv->pwm_period; }
static inline eDeviceStatus gate_drv_get_status(const tGateDrv *drv) { return drv->dstate; }

#endif // __GATE_DRV_H
