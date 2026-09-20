#ifndef __LED_WS28XX_H
#define __LED_WS28XX_H

#include "led.h"

// ============================================================
// led_ws28xx.h — WS2812/WS28xx RGB 灯驱动（板级，PWM + DMA）
//
// 实现 abs/led.h 的 tRgbOps，并导出驱动实例供装配层挂钩。
// ============================================================

// 驱动实例（布局私有：只暴露符号，装配层仅取地址）
typedef struct tWs28xx tWs28xx;
extern tWs28xx g_ws28xx;

// 驱动 ops（abs/led.h 的 tRgbOps）
extern const tRgbOps ws28xx_ops;

#endif // __LED_WS28XX_H
