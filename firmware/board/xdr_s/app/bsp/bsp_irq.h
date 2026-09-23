#ifndef __BSP_IRQ_H
#define __BSP_IRQ_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32g4xx_hal.h"

// 控制基频（由板级 bsp_irq.c 定义）
extern const float F_CON; // 控制基频 (Hz)
extern const float T_CON; // 控制周期 (s)

// 全局中断开关 / 系统复位
void irq_enable(void);
void irq_disable(void);
void system_reset(void);

// 中断注册
void pwm_register_callback(void (*up_cb)(void), void (*down_cb)(void));

#endif // __BSP_IRQ_H
