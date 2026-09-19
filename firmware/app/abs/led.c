// ============================================================
// led.c — LED / RGB 业务编排（abs，纯逻辑）
//
// tLed：开/关/慢闪/快闪（按时间切换，toggle 板级）
// tRgb：灭/亮/慢闪/快闪/呼吸（正弦亮度曲线）
// 颜色常量与 64 点正弦表定义于此。
//
// 硬件动作经板级钩子 led_board.h 直接调用，无 ops 表。
// ============================================================

#include "led.h"

#include "IF_time.h"
#include "led_board.h"

// ---- 预置颜色 ----
const tRGBColor RGB_BLACK = {0, 0, 0};
const tRGBColor RGB_WHITE = {255, 255, 255};
const tRGBColor RGB_RED = {255, 0, 0};
const tRGBColor RGB_GREEN = {0, 255, 0};
const tRGBColor RGB_BLUE = {0, 0, 255};
const tRGBColor RGB_YELLOW = {255, 255, 0};

// 64 点正弦表（0~255 亮度）
static const uint8_t SINE_TABLE[64] = {
    128, 140, 153, 165, 177, 188, 199, 209,
    218, 226, 234, 240, 245, 250, 253, 254,
    254, 253, 250, 245, 240, 234, 226, 218,
    209, 199, 188, 177, 165, 153, 140, 128,
    128, 116, 103, 91, 79, 68, 57, 47,
    38, 30, 22, 16, 11, 6, 3, 2,
    2, 3, 6, 11, 16, 22, 30, 38,
    47, 57, 68, 79, 91, 103, 116, 128};

// ==================== tLed ====================

bool led_init(tLed *led, uint8_t idx)
{
    if (!led)
        return false;

    memset(led, 0, sizeof(tLed));
    led->idx = idx;
    led->state = LED_OFF;
    led->fast_ms = 300U; // 默认：快速闪烁 300ms 半周期
    led->slow_ms = 800U; // 默认：慢速闪烁 800ms 半周期
    return led_board_open(idx);
}

void led_set_state(tLed *led, eLedState state)
{
    if (!led)
        return;
    led->state = state;
    led->next_change_ms = 0U; // 强制下个 task 立即生效
}

void led_set_times(tLed *led, uint16_t fast_ms, uint16_t slow_ms)
{
    if (!led)
        return;
    if (fast_ms != 0U)
        led->fast_ms = fast_ms;
    if (slow_ms != 0U)
        led->slow_ms = slow_ms;
}

void led_task(tLed *led)
{
    if (!led)
        return;

    switch (led->state)
    {
    case LED_ON:
        led_board_set(led->idx, true);
        break;
    case LED_OFF:
        led_board_set(led->idx, false);
        break;
    case LED_BLINK_SLOW:
    case LED_BLINK_FAST:
    {
        uint32_t now = time_get_ms();
        uint32_t half = (led->state == LED_BLINK_FAST) ? led->fast_ms : led->slow_ms;
        if ((now - led->next_change_ms) >= half)
        {
            led_board_toggle(led->idx);
            led->next_change_ms = now;
        }
        break;
    }
    default:
        break;
    }
}

// ==================== tRgb ====================

bool rgb_init(tRgb *rgb)
{
    if (!rgb)
        return false;

    memset(rgb, 0, sizeof(tRgb));
    rgb->state = RGB_OFF;
    rgb->color = RGB_BLACK;
    rgb->fast_ms = 300U;
    rgb->slow_ms = 800U;
    rgb->breathe_ms = 40U;
    return rgb_board_open();
}

void rgb_set_state(tRgb *rgb, eRgbState state)
{
    if (!rgb)
        return;
    rgb->state = state;
    rgb->next_change_ms = 0U;
}

void rgb_set_color(tRgb *rgb, tRGBColor color)
{
    if (!rgb)
        return;
    rgb->color = color;
    rgb_board_set_color(color); // 颜色缓存到驱动，亮度由 task 驱动
}

void rgb_set_times(tRgb *rgb, uint16_t fast_ms, uint16_t slow_ms, uint16_t breathe_ms)
{
    if (!rgb)
        return;
    if (fast_ms != 0U)
        rgb->fast_ms = fast_ms;
    if (slow_ms != 0U)
        rgb->slow_ms = slow_ms;
    if (breathe_ms != 0U)
        rgb->breathe_ms = breathe_ms;
}

void rgb_task(tRgb *rgb)
{
    if (!rgb)
        return;

    switch (rgb->state)
    {
    case RGB_ON:
        rgb_board_set_brightness(255U);
        rgb_board_refresh();
        break;
    case RGB_OFF:
        rgb_board_set_brightness(0U);
        rgb_board_refresh();
        break;
    case RGB_BLINK_SLOW:
    case RGB_BLINK_FAST:
    {
        uint32_t now = time_get_ms();
        uint32_t half = (rgb->state == RGB_BLINK_FAST) ? rgb->fast_ms : rgb->slow_ms;
        if ((now - rgb->next_change_ms) >= half)
        {
            // 以 0/255 两档切换亮度
            uint8_t b = (rgb->breath_idx & 1U) ? 0U : 255U;
            rgb_board_set_brightness(b);
            rgb_board_refresh();
            rgb->breath_idx++;
            rgb->next_change_ms = now;
        }
        break;
    }
    case RGB_BREATHE:
    {
        uint32_t now = time_get_ms();
        if ((now - rgb->next_change_ms) >= (uint32_t)rgb->breathe_ms)
        {
            rgb_board_set_brightness(SINE_TABLE[rgb->breath_idx]);
            rgb_board_refresh();
            rgb->breath_idx = (uint8_t)((rgb->breath_idx + 1U) & 63U);
            rgb->next_change_ms = now;
        }
        break;
    }
    default:
        break;
    }
}
