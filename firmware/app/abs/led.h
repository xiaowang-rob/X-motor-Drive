#ifndef __LED_H
#define __LED_H

#include "device.h"

// ============================================================
// led.h — LED / RGB 灯业务编排（abs）
//
// 接口形态：ops + handle（装配层在定义 tLed / tRgb 对象时挂上）。
// 业务对象只做闪烁/呼吸/颜色编排，引脚、极性与时序细节不在本层。
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

// 驱动接口（板级实现）；handle 为驱动实例（= 第几颗 LED）
typedef struct
{
    bool (*open)(void *handle);             // 初始化该颗 LED
    void (*set)(void *handle, bool active); // 点亮 / 熄灭（极性由驱动处理）
    void (*toggle)(void *handle);           // 翻转
} tLedOps;

typedef struct
{
    const tLedOps *ops; // 驱动 ops（装配时挂）
    void *handle;       // 驱动实例（装配时挂）

    volatile eLedState state; // 目标状态
    uint16_t fast_ms;         // 快速闪烁半周期
    uint16_t slow_ms;         // 慢速闪烁半周期
    uint32_t next_change_ms;  // 下次翻转时刻
} tLed;

// 启动对象：ops/handle 须已由装配层挂好（任一为空返回 false）
bool led_init(tLed *led);
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

// 驱动接口（板级实现）；handle 为驱动实例
typedef struct
{
    bool (*open)(void *handle);                          // 初始化 RGB 灯串
    void (*set_color)(void *handle, tRGBColor color);    // 颜色缓存到驱动
    void (*set_brightness)(void *handle, uint8_t value); // 亮度缓存到驱动
    void (*refresh)(void *handle);                       // 颜色+亮度推给硬件（忙则丢弃本次）
} tRgbOps;

typedef struct
{
    const tRgbOps *ops; // 驱动 ops（装配时挂）
    void *handle;       // 驱动实例（装配时挂）

    volatile eRgbState state;
    tRGBColor color;

    uint16_t fast_ms;        // 快速闪烁半周期
    uint16_t slow_ms;        // 慢速闪烁半周期
    uint16_t breathe_ms;     // 呼吸步进间隔
    uint32_t next_change_ms; // 下一次切换时间
    uint8_t breath_idx;      // 呼吸查表索引（0~63）
} tRgb;

// 启动对象：ops/handle 须已由装配层挂好（任一为空返回 false）
bool rgb_init(tRgb *rgb);
void rgb_set_state(tRgb *rgb, eRgbState state);
void rgb_set_color(tRgb *rgb, tRGBColor color);
void rgb_set_times(tRgb *rgb, uint16_t fast_ms, uint16_t slow_ms, uint16_t breathe_ms);
void rgb_task(tRgb *rgb); // 周期调用，按状态驱动硬件（含刷新）

#endif // __LED_H
