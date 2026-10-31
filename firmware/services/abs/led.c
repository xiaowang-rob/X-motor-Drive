#include "led.h"

extern uint32_t bsp_time_ms(void);

#define LED_SHORT_MS 50
#define LED_LONG_MS 300
#define LED_GAP_MS 400
#define RGB_STEP_MS 40

// 状态顺序与 eIndicator_state 一致；GPIO 使用闪烁次数，RGB 使用颜色。
static const struct {
    uint8_t pulses;
    tRGBColor color;
} EFFECTS[] = {
    {1U, {0, 0, 255}},
    {2U, {0, 255, 0}},
    {3U, {255, 255, 0}},
    {1U, {255, 0, 0}},
};

// 一周期 64 点，从暗到亮再到暗。
static const uint8_t BREATH[64] = {
    0,   1,   2,   6,   10,  15,  21,  29,  37,  46,  57,  67,  79,
    90,  103, 115, 128, 140, 152, 165, 176, 188, 198, 209, 218, 226,
    234, 240, 245, 249, 253, 254, 255, 254, 253, 249, 245, 240, 234,
    226, 218, 209, 198, 188, 176, 165, 152, 140, 128, 115, 103, 90,
    79,  67,  57,  46,  37,  29,  21,  15,  10,  6,   2,   1,
};

static bool valid_state(eIndicator_state state) {
    return (unsigned)state <= INDICATOR_STATE_FAULT;
}

bool led_init(tLed *led, const tLedDevHandle *handle) {
    if (!led)
        return false;
    *led = (tLed){0};
    if (!handle || !handle->ops || !handle->ops->open || !handle->ops->set ||
        (handle->type != LED_TYPE_LED && handle->type != LED_TYPE_RGB))
        return false;
    if (!handle->ops->open(handle->dev))
        return false;
    led->handle = handle;
    led->state = INDICATOR_STATE_IDLE;
    handle->ops->set(handle->dev, (tRGBColor){0, 0, 0}, 0U);
    return true;
}

void led_set_state(tLed *led, eIndicator_state state) {
    if (!led || !valid_state(state) || led->state == state)
        return;
    led->state = state;
    led->started = false;
}

void led_task(tLed *led) {
    if (!led || !led->handle || !valid_state(led->state))
        return;

    uint32_t now = bsp_time_ms();
    if (!led->started) {
        led->cycle_start_ms = now;
        led->started = true;
    }

    uint32_t on_ms =
        led->state == INDICATOR_STATE_IDLE ? LED_LONG_MS : LED_SHORT_MS;
    uint32_t pulses = EFFECTS[led->state].pulses;
    uint32_t period = on_ms + (pulses - 1U) * 2U * LED_SHORT_MS + LED_GAP_MS;
    bool breathe = led->handle->type == LED_TYPE_RGB &&
                   led->state != INDICATOR_STATE_FAULT;
    if (breathe)
        period = sizeof(BREATH) * RGB_STEP_MS;
    else if (led->state == INDICATOR_STATE_FAULT)
        period = 2U * LED_SHORT_MS;

    // 无符号差值处理毫秒计数回绕；跳过漏调的完整周期，保持节奏。
    uint32_t elapsed = now - led->cycle_start_ms;
    led->cycle_start_ms += (elapsed / period) * period;
    elapsed %= period;

    uint8_t brightness;
    if (breathe) {
        brightness = BREATH[elapsed / RGB_STEP_MS];
    } else {
        uint32_t span = on_ms + (pulses - 1U) * 2U * LED_SHORT_MS;
        bool on = elapsed < span && elapsed % (on_ms + LED_SHORT_MS) < on_ms;
        brightness = on ? 255U : 0U;
    }
    led->handle->ops->set(
        led->handle->dev, EFFECTS[led->state].color, brightness);
}
