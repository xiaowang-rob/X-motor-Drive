#ifndef XDR_APP_ABS_LED_BOARD_H
#define XDR_APP_ABS_LED_BOARD_H

#include "device.h"
#include "led.h"

// ============================================================
// led_board.h — LED / RGB 的板级钩子契约（abs 声明 / 板级实现）
//
// 实现 = board/<B>/drv/led_gpio.c（普通 LED）与 led_ws28xx.c（可编程 RGB）。
// 引脚、极性、PWM 通道、时序编码全部在板级，abs 不感知。
//
// 语义：
//   led_board_open(idx)      初始化第 idx 颗 LED；idx 越界返回 false
//   led_board_set(idx, on)   点亮 / 熄灭（极性由板级处理）
//   led_board_toggle(idx)    翻转
//   rgb_board_open()         初始化 RGB 灯串
//   rgb_board_set_color()    设置颜色（缓存到驱动）
//   rgb_board_set_brightness() 设置亮度（缓存到驱动）
//   rgb_board_refresh()      把当前颜色+亮度推给硬件（DMA 忙则丢弃本次）
// ============================================================

bool led_board_open(uint8_t idx);
void led_board_set(uint8_t idx, bool active);
void led_board_toggle(uint8_t idx);

bool rgb_board_open(void);
void rgb_board_set_color(tRGBColor color);
void rgb_board_set_brightness(uint8_t brightness);
void rgb_board_refresh(void);

#endif // XDR_APP_ABS_LED_BOARD_H
