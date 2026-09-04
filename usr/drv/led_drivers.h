#ifndef __LED_DRIVERS_H
#define __LED_DRIVERS_H

#include "led.h"

// ============================================================
// led_drivers.h — 板载 GPIO LED /PWM RGB 等 LED驱动接口
//
// ============================================================

// ---- LED 系列（GPIO 控制） ----
const tLedDriverOps *led_drv_ops(void);
LedHandle led_drv_handle(uint8_t idx);

// ---- WS2812/WS28xx 系列（GRB，PWM 时序驱动） ----
RgbHandle ws28xx_create(void);
void ws28xx_destroy(RgbHandle h);
extern const tRgbDriverOps ws28xx_driver_ops;

#endif // __LED_DRIVERS_H
