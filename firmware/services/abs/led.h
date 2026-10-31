#ifndef __LED_H
#define __LED_H

#include "device_status.h"

typedef enum {
    LED_TYPE_LED = 0, // 普通 LED
    LED_TYPE_RGB,     // RGB 灯串
} eLedType;

typedef enum {
    INDICATOR_STATE_IDLE,     // LED 单长闪，RGB 蓝呼吸
    INDICATOR_STATE_RUNNING0, // LED 双短闪，RGB 绿呼吸
    INDICATOR_STATE_RUNNING1, // LED 三短闪，RGB 黄呼吸
    INDICATOR_STATE_FAULT,    // LED 连续快短闪，RGB 红色闪烁
} eIndicator_state;

typedef struct {
    uint8_t R;
    uint8_t G;
    uint8_t B;
} tRGBColor;

// set 一次完成输出；普通 LED 忽略 color，brightness 非零即点亮。
// RGB 驱动负责将 color 与 brightness（0~255）合成并刷新硬件。
typedef struct {
    bool (*open)(void *handle);
    void (*set)(void *handle, tRGBColor color, uint8_t brightness);
} tLedOps;

typedef struct {
    const tLedOps *ops;
    void *dev;
    eLedType type;
} tLedDevHandle;

typedef struct {
    const tLedDevHandle *handle;
    eIndicator_state state;
    uint32_t cycle_start_ms;
    bool started;
} tLed;

// 全局变量，初始化后熄灭；首次 task 从 IDLE 效果开始。
extern tLed g_led;

// 初始化后熄灭；首次 task 从 IDLE 效果开始。
bool led_init(tLed *led, const tLedDevHandle *handle);
// 相同状态不重启节奏；新状态在下次 task 生效。
void led_set_state(tLed *led, eIndicator_state state);
void led_task(tLed *led); // 呼吸效果建议每 40 ms 或更频繁调用

#endif // __LED_H
