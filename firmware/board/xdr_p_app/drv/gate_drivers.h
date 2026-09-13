#ifndef __MOTOR_DRV_H
#define __MOTOR_DRV_H

#include "gate_drv.h"

// ============================================================
// gate_drivers.h — 本板栅极驱动出口
//
// TIM8 中心对齐 6 路 PWM（CH1-3 + 互补）+ 12V 电源 + FOC 节拍中断。
// 驱动以"文件内静态 handle 实例"存在，本头只暴露 ops、取实例函数，
// 以及 FOC 节拍回调的注册入口。
//
// 注：HAL_TIM_PeriodElapsedCallback 由 gate_fd6288q.c 唯一持有
//     （TIM8 是该文件的资源），上溢/下溢分别转发到注册的回调。
// ============================================================

extern const tGateDrvOps gate_fd6288q_ops;

// 取栅极驱动实例句柄（静态实例，见 gate_fd6288q.c）
GateHandle gate_get_handle(void);

// FOC 节拍回调注册：
//   sample_cb —— 上溢（2-shunt 电流采样点）
//   ctrl_cb   —— 下溢（FOC 控制主循环）
void gate_register_isrs(void (*sample_cb)(void), void (*ctrl_cb)(void));

#endif // __MOTOR_DRV_H
