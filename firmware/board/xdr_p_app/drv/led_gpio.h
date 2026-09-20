#ifndef __LED_GPIO_H
#define __LED_GPIO_H

#include "led.h"

// ============================================================
// led_gpio.h — 板载 GPIO LED 驱动（板级）
//
// 实现 abs/led.h 的 tLedOps；每颗 LED 一个实例，装配层用
// &g_led_gpio_0 / &g_led_gpio_1 作为该颗 LED 的 handle。
// 引脚映射与极性只出现在实现文件里。
// ============================================================

// 驱动实例（布局私有：只暴露符号，装配层仅取地址）
typedef struct tLedGpio tLedGpio;
extern tLedGpio g_led_gpio_0; // 板载 LED0
extern tLedGpio g_led_gpio_1; // 板载 LED1

// 驱动 ops（abs/led.h 的 tLedOps）
extern const tLedOps led_gpio_ops;

#endif // __LED_GPIO_H
