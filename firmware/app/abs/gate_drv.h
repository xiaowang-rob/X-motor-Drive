#ifndef XDR_APP_ABS_GATE_DRV_H
#define XDR_APP_ABS_GATE_DRV_H

#include "device.h"

// ============================================================
// gate_drv.h — 栅极驱动 / 电机功率级业务对象（abs）
//
// 编译期绑定：硬件动作经板级钩子（gate_drv_board.h）直接调用，
// 无 ops 表、无 void* 句柄。本层只维护状态并转发。
//
// 热路径提示：gate_drv_set_compare 由 FOC 下溢中断以 20kHz 调用，
// 调用链为 ctl → gate_drv_set_compare → gate_board_set_compare（均直接调用）。
// 若需零开销，对 abs 与板级同时开启 LTO 即可整链内联。
// ============================================================

typedef struct
{
    uint32_t pwm_period;  // PWM 周期计数（init 时自板级取回）
    eDeviceStatus dstate; // 设备状态
} tGateDrv;

// 绑定并初始化（内部调用板级钩子 gate_board_open）
bool gate_drv_init(tGateDrv *drv);

void gate_drv_power_on(tGateDrv *drv, bool on);
void gate_drv_enable(tGateDrv *drv, bool en);
void gate_drv_set_compare(tGateDrv *drv, uint16_t ticA, uint16_t ticB, uint16_t ticC);

// FOC 节拍回调注册（转发给板级；TIM8 中断由 bsp_irq 统一分发）
//   sample_cb —— 下溢（2-shunt 电流采样点）
//   ctrl_cb   —— 上溢（FOC 控制主循环）
void gate_drv_register_isrs(void (*sample_cb)(void), void (*ctrl_cb)(void));

static inline uint32_t gate_drv_get_pwm_period(const tGateDrv *drv) { return drv->pwm_period; }
static inline eDeviceStatus gate_drv_get_status(const tGateDrv *drv) { return drv->dstate; }

#endif // XDR_APP_ABS_GATE_DRV_H
