#ifndef __LED_H
#define __LED_H

#include "device.h"

// ============================================================
// led.h — LED / RGB 灯业务编排（abs）
//
// 接口形态：ops + handle（装配层在定义 tLed / tRgb 对象时挂上）。
// 业务对象只做闪烁节奏 / 呼吸 / 颜色编排，引脚、极性与时序细节不在本层。
//
// 闪烁节奏（LED 与 RGB 共用一套）：
//   单长闪 / 双短闪 / 三短闪 / 短+长 / 连续短闪
// 每个节奏 = 若干「段」，每段 = 亮一档 → 灭一档；
// 末段的灭档就是该节奏播完后的循环间隔，整体一直循环重复。
// 节奏表只描述档位（短/长/间隔），对应毫秒是编译期常量（led.c 的 LED_DEF_*）。
//
// RGB = 同一套闪烁节奏 + 颜色 + 呼吸。
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

// ==================== 闪烁节奏（LED / RGB 共用）====================

// 时长档：具体毫秒是编译期常量（led.c 的 LED_DEF_*）
typedef enum
{
    LED_T_SHORT = 0, // 短闪档
    LED_T_LONG,      // 长闪档
    LED_T_GAP        // 循环间隔档
} eLedTime;

// 一段：亮 on 档（毫秒）→ 灭 off 档（毫秒）；末段的 off 档即循环间隔
typedef struct
{
    eLedTime on;
    eLedTime off;
} tLedPulse;

typedef struct
{
    const tLedPulse *pulses; // 段序列
    uint8_t count;           // 段数（≥ 1）
} tLedPattern;

// 节奏相位（业务对象内部推进用）
typedef enum
{
    LED_PHASE_IDLE = 0, // 待启动：下个 task 点亮首段
    LED_PHASE_ON,       // 亮中
    LED_PHASE_OFF       // 灭中
} eLedPhase;

// ==================== 普通 LED ====================

typedef enum
{
    LED_OFF = 0,
    LED_ON,
    LED_BLINK_LONG,         // 单长闪
    LED_BLINK_DOUBLE_SHORT, // 双短闪
    LED_BLINK_TRIPLE_SHORT, // 三短闪
    LED_BLINK_SHORT_LONG,   // 短 + 长
    LED_BLINK_CONT_SHORT    // 连续短闪
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

    uint8_t pulse_idx;       // 当前段
    eLedPhase phase;         // 当前相位
    uint32_t next_change_ms; // 当前相位结束时刻
} tLed;

// 启动对象：ops/handle 须已由装配层挂好（任一为空返回 false）
bool led_init(tLed *led);
void led_set_state(tLed *led, eLedState state);
void led_task(tLed *led); // 周期调用，按状态驱动硬件

// ==================== RGB ====================

// RGB_BLINK_* 与同名 LED_BLINK_* 数值一一对齐，共用同一套节奏表
typedef enum
{
    RGB_OFF = 0,
    RGB_ON,
    RGB_BLINK_LONG,         // 单长闪
    RGB_BLINK_DOUBLE_SHORT, // 双短闪
    RGB_BLINK_TRIPLE_SHORT, // 三短闪
    RGB_BLINK_SHORT_LONG,   // 短 + 长
    RGB_BLINK_CONT_SHORT,   // 连续短闪
    RGB_BREATHE             // RGB 专属：呼吸
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

    uint8_t pulse_idx;       // 当前段
    eLedPhase phase;         // 当前相位
    uint8_t breath_idx;      // 呼吸查表索引（0~63）
    uint32_t next_change_ms; // 当前相位结束时刻
} tRgb;

// 启动对象：ops/handle 须已由装配层挂好（任一为空返回 false）
bool rgb_init(tRgb *rgb);
void rgb_set_state(tRgb *rgb, eRgbState state);
void rgb_set_color(tRgb *rgb, tRGBColor color);
void rgb_task(tRgb *rgb); // 周期调用，按状态驱动硬件（含刷新）

#endif // __LED_H
