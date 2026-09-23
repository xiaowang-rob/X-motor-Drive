#ifndef __BSP_IRQ_H
#define __BSP_IRQ_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32g4xx_hal.h"

// ============================================================
// bsp_irq.h — 中断基础设施（板级 bsp）
//
// 本模块只提供中断相关的公共设施：
//   - F_CON / T_CON：控制基频常量（由功率级 PWM 决定）
//   - irq_enable / irq_disable / system_reset：全局中断开关与复位
//
// 约定：HAL_*_Callback 由**需要它的驱动文件自行定义**（本板每类外设当前
//       只有一路），不再走集中注册表转发 —— 不为一个单点符号引入额外
//       间接层。若同类外设出现第二路，在回调内按 Instance 区分即可。
// 中断上下文约束：回调内不得阻塞、不得用 HAL_Delay。
// ============================================================

// 控制基频（由板级 bsp_irq.c 定义）
extern const float F_CON; // 控制基频 (Hz)
extern const float T_CON; // 控制周期 (s)

// 全局中断开关 / 系统复位
void irq_enable(void);
void irq_disable(void);
void system_reset(void);

#endif // __BSP_IRQ_H
