#ifndef __LED_DRIVERS_H
#define __LED_DRIVERS_H

#include "led.h"

// ============================================================
// led_drivers.h — 本板 LED 驱动出口
//
//   - GPIO LED：板载两颗普通 LED（CAN 状态灯 / 编码器状态灯）
//   - RBG 灯串：WS2812（PWM + DMA 时序驱动）
//
// 每个驱动以"文件内静态 handle 实例"存在，本头只暴露 ops 与取实例函数。
// ============================================================

// ---- GPIO LED 系列 ----
extern const tLedDriverOps led_drv_ops;

// 取第 idx 颗 LED 实例句柄（idx 超范围返回 NULL）
LedHandle led_get_handle(uint8_t idx);

// ---- WS2812/WS28xx RGB 系列（GRB，PWM 时序驱动） ----
extern const tRgbDriverOps rgb_ws28xx_ops;

// 取 RGB 实例句柄（静态实例，见 led_ws28xx.c）
RgbHandle rgb_get_handle(void);

#endif // __LED_DRIVERS_H
