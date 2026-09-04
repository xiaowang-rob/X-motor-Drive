#ifndef __LED_DRIVERS_H
#define __LED_DRIVERS_H

#include <stdint.h>

#include "usr/abs/led.h"

// ============================================================
// led_drivers.h — 板载 GPIO LED 驱动（usr/drv，v2 直连版）
//
// 板上普通 LED 的 tLedDriverOps 实现：引脚/极性在驱动内（经 platform.h）。
// ============================================================

const tLedDriverOps *led_drv_ops(void);
LedHandle led_drv_handle(uint8_t idx); // idx0=CAN 灯，idx1=编码器灯

#endif // __LED_DRIVERS_H
