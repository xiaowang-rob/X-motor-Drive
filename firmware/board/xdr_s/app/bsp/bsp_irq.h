#ifndef __BSP_IRQ_H
#define __BSP_IRQ_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32g4xx_hal.h"

// 全局中断开关 / 系统复位
void irq_enable(void);
void irq_disable(void);
void system_reset(void);

// 中断注册
void pwm_register_callback(void (*up_cb)(void), void (*down_cb)(void));

#endif // __BSP_IRQ_H
