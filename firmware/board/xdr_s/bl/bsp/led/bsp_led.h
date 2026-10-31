#ifndef __BSP_LED_H
#define __BSP_LED_H

#include "led.h"

// 驱动实例
const tLedDevHandle *Get_Gpio_LEDHandle(void);

// 驱动 ops（abs/led.h 的 tLedOps）
extern const tLedOps led_gpio_ops;

#endif // __BSP_LED_H
