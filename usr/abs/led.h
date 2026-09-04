#ifndef __ABS_LED_H
#define __ABS_LED_H

#include <stdint.h>
#include <stdbool.h>

#include "usr/abs/time.h"

// ============================================================
// led.h — LED / RGB 灯（usr/abs）
//
// 分两部分：
//   [驱动契约]  面向硬件灯效的最小能力（板级 GPIO LED / 可编程 RGB）
//   [业务对象]  tLed/tRgb：闪烁/呼吸/颜色编排，时间来自注入的 tTimeIf
//
// 规则：业务对象只依赖 ops 契约与时间接口，不感知引脚/芯片/板级资源。
// ============================================================

// ==================== 颜色 ====================

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

// ==================== 驱动契约 ====================

// 普通 LED（GPIO 亮灭）
typedef void *LedHandle;

typedef struct
{
    bool (*init)(LedHandle h);
    void (*set)(LedHandle h, bool active);
    void (*toggle)(LedHandle h);
} tLedDriverOps;

// 可编程 RGB（颜色+亮度+刷新）
typedef void *RgbHandle;

typedef struct
{
    bool (*init)(RgbHandle h);
    void (*set_rgb)(RgbHandle h, tRGBColor color);
    void (*set_brightness)(RgbHandle h, uint8_t brightness);
    void (*refresh)(RgbHandle h);
} tRgbDriverOps;

// ==================== 业务对象 ====================

// ---- 普通 LED 状态 ----
typedef enum
{
    LED_OFF,
    LED_ON,
    LED_BLINK_SLOW,
    LED_BLINK_FAST
} eLedState;

typedef struct
{
    const tLedDriverOps *ops;
    LedHandle handle;
    const tTimeIf *time; // 注入：时间基准

    volatile eLedState state;

    uint16_t fast_ms; // 快速闪烁半周期
    uint16_t slow_ms; // 慢速闪烁半周期
    uint32_t next_change_ms;
} tLed;

bool led_init(tLed *led, const tLedDriverOps *ops, LedHandle handle, const tTimeIf *time);
void led_set_state(tLed *led, eLedState state);
void led_set_times(tLed *led, uint16_t fast_ms, uint16_t slow_ms);
void led_task(tLed *led); // 周期调用，按状态驱动硬件

// ---- RGB 状态 ----
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
    const tRgbDriverOps *ops;
    RgbHandle handle;
    const tTimeIf *time; // 注入：时间基准

    volatile eRgbState state;
    tRGBColor color;

    uint16_t fast_ms;         // 快速闪烁半周期
    uint16_t slow_ms;         // 慢速闪烁半周期
    uint16_t breathe_ms;      // 呼吸步进间隔
    uint32_t next_change_ms;  // 下一次切换时间
    uint8_t breath_idx;       // 呼吸查表索引（0~63）
} tRgb;

bool rgb_init(tRgb *rgb, const tRgbDriverOps *ops, RgbHandle handle, const tTimeIf *time);
void rgb_set_state(tRgb *rgb, eRgbState state);
void rgb_set_color(tRgb *rgb, tRGBColor color);
void rgb_set_times(tRgb *rgb, uint16_t fast_ms, uint16_t slow_ms, uint16_t breathe_ms);
void rgb_task(tRgb *rgb); // 周期调用，按状态驱动硬件

#endif // __ABS_LED_H
