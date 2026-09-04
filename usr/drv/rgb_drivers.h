#ifndef __RGB_DRIVERS_H
#define __RGB_DRIVERS_H

#include "usr/abs/led.h"

// ============================================================
// rgb_drivers.h — RGB 灯芯片驱动统一出口（usr/drv，v2 直连版）
//
// v2：驱动直接用本板 PWM-DMA（经 platform.h），create 无参，
// 灯珠数取 platform 的 Pixel_NUM。
// ============================================================

// ---- WS2812/WS28xx 系列（GRB，PWM 时序驱动） ----
RgbHandle ws28xx_create(void);
void ws28xx_destroy(RgbHandle h);
extern const tRgbDriverOps ws28xx_driver_ops;

#endif // __RGB_DRIVERS_H
