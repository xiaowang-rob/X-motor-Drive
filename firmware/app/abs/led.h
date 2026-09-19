#ifndef XDR_APP_ABS_LED_H
#define XDR_APP_ABS_LED_H

#include "device.h"

// ============================================================
// led.h — LED / RGB 灯业务编排（abs）
//
// 编译期绑定：硬件动作经板级钩子（led_board.h）直接调用，
// 无 ops 表、无 void* 句柄。业务对象只做闪烁/呼吸/颜色编排，
// 时间与板级资源细节不在本层。
// ============================================================

typedef struct
{
    uint8_t R;
    uint8_t G;
    uint8_t B;
} tRGBColor;

// 预置颜色（定义于 led.c）
extern const tRGBColor RGB_BLACK;
extern const tRGBColor RGB_WHITE;
extern const tRGBColor RGB_RED;
extern const tRGBColor RGB_GREEN;
extern const tRGBColor RGB_BLUE;
extern const tRGBColor RGB_YELLOW;

// ==================== 普通 LED ====================

typedef enum
{
    LED_OFF,
    LED_ON,
    LED_BLINK_SLOW,
    LED_BLINK_FAST
} eLedState;

typedef struct
{
    uint8_t idx;                 // 板级实例索引（0 起）
    volatile eLedState state;    // 目标状态
    uint16_t fast_ms;            // 快速闪烁半周期
    uint16_t slow_ms;            // 慢速闪烁半周期
    uint32_t next_change_ms;     // 下次翻转时刻
} tLed;

// 绑定板级第 idx 颗 LED 并初始化
bool led_init(tLed *led, uint8_t idx);
void led_set_state(tLed *led, eLedState state);
void led_set_times(tLed *led, uint16_t fast_ms, uint16_t slow_ms);
void led_task(tLed *led); // 周期调用，按状态驱动硬件

// ==================== RGB ====================

typedef enum
{
    RGB_OFF,
    RGB_ON,
    RGB_BLINK_SLOW,
    RGB_BLINK_FAST,
    RGB_BREATHE
} eRgbState;

typedef struct
{
    volatile eRgbState state;
    tRGBColor color;

    uint16_t fast_ms;        // 快速闪烁半周期
    uint16_t slow_ms;        // 慢速闪烁半周期
    uint16_t breathe_ms;     // 呼吸步进间隔
    uint32_t next_change_ms; // 下一次切换时间
    uint8_t breath_idx;      // 呼吸查表索引（0~63）
} tRgb;

bool rgb_init(tRgb *rgb);
void rgb_set_state(tRgb *rgb, eRgbState state);
void rgb_set_color(tRgb *rgb, tRGBColor color);
void rgb_set_times(tRgb *rgb, uint16_t fast_ms, uint16_t slow_ms, uint16_t breathe_ms);
void rgb_task(tRgb *rgb); // 周期调用，按状态驱动硬件（含刷新）

#endif // XDR_APP_ABS_LED_H
