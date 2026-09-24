// ============================================================
// led.c — LED / RGB 业务编排（abs，纯逻辑）
//
// tLed：灭/亮 + 5 种闪烁节奏（单长闪、双短闪、三短闪、短+长、连续短闪）
// tRgb：同一套节奏 + 颜色 + 呼吸（正弦亮度曲线）
// 颜色常量、节奏表与 64 点正弦表定义于此。
//
// 节奏以「段序列」描述：每段 = 亮一档 → 灭一档，末段的灭档即循环间隔；
// 整轮播完回到首段，循环重复。档位（短/长/间隔）对应的毫秒是编译期常量，
// 见下方 LED_DEF_*。
//
// 硬件动作经 ops + handle（装配时挂）直达驱动。
// ============================================================

#include "led.h"

#include "bsp_time.h"

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

// ---- 档位时长（编译期常量，须非 0）----
#define LED_DEF_SHORT_MS 200U  // 短闪档
#define LED_DEF_LONG_MS 800U   // 长闪档
#define LED_DEF_GAP_MS 1000U   // 循环间隔档
#define RGB_DEF_BREATHE_MS 40U // 呼吸步进

// ---- 节奏表（LED / RGB 共用）----

// 单长闪：长
static const tLedPulse P_LONG[] = {
    {LED_T_LONG, LED_T_GAP},
};

// 双短闪：短 短
static const tLedPulse P_DOUBLE_SHORT[] = {
    {LED_T_SHORT, LED_T_SHORT},
    {LED_T_SHORT, LED_T_GAP},
};

// 三短闪：短 短 短
static const tLedPulse P_TRIPLE_SHORT[] = {
    {LED_T_SHORT, LED_T_SHORT},
    {LED_T_SHORT, LED_T_SHORT},
    {LED_T_SHORT, LED_T_GAP},
};

// 短 + 长
static const tLedPulse P_SHORT_LONG[] = {
    {LED_T_SHORT, LED_T_SHORT},
    {LED_T_LONG, LED_T_GAP},
};

// 连续短闪：段间隔即循环间隔，无限重复
static const tLedPulse P_CONT_SHORT[] = {
    {LED_T_SHORT, LED_T_SHORT},
};

static const tLedPattern PAT_LONG = {P_LONG, 1U};
static const tLedPattern PAT_DOUBLE_SHORT = {P_DOUBLE_SHORT, 2U};
static const tLedPattern PAT_TRIPLE_SHORT = {P_TRIPLE_SHORT, 3U};
static const tLedPattern PAT_SHORT_LONG = {P_SHORT_LONG, 2U};
static const tLedPattern PAT_CONT_SHORT = {P_CONT_SHORT, 1U};

// 状态值 → 节奏表；LED_BLINK_* 与 RGB_BLINK_* 数值对齐，故共用此查表
static const tLedPattern *pattern_of(uint8_t state)
{
    switch (state)
    {
    case LED_BLINK_LONG:
        return &PAT_LONG;
    case LED_BLINK_DOUBLE_SHORT:
        return &PAT_DOUBLE_SHORT;
    case LED_BLINK_TRIPLE_SHORT:
        return &PAT_TRIPLE_SHORT;
    case LED_BLINK_SHORT_LONG:
        return &PAT_SHORT_LONG;
    case LED_BLINK_CONT_SHORT:
        return &PAT_CONT_SHORT;
    default:
        return NULL;
    }
}

// ---- 节奏相位推进（LED / RGB 共用，不碰硬件）----

typedef struct
{
    const tLedPattern *pat;
    uint8_t *pulse_idx;
    eLedPhase *phase;
    uint32_t *next_change_ms;
} tBlinkCtx;

static bool phase_elapsed(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0; // 差比较，抗 32 位回绕
}

// 档位 → 毫秒（编译期常量；宏须非 0，否则该相位会每拍紧翻转）
static uint32_t level_ms(eLedTime level)
{
    switch (level)
    {
    case LED_T_SHORT:
        return LED_DEF_SHORT_MS;
    case LED_T_LONG:
        return LED_DEF_LONG_MS;
    default:
        return LED_DEF_GAP_MS;
    }
}

// 推进一次相位机。
// 返回 1 = 本拍应点亮，0 = 本拍应熄灭，-1 = 本拍无需动作。
static int8_t blink_step(tBlinkCtx *ctx)
{
    if (!ctx || !ctx->pat || ctx->pat->count == 0U)
        return -1;

    uint32_t now = time_get_ms();

    // 待启动：点亮首段（set_state 后由首个 task 生效，不在 set_state 里碰硬件）
    if (*ctx->phase == LED_PHASE_IDLE)
    {
        *ctx->pulse_idx = 0U;
        *ctx->phase = LED_PHASE_ON;
        *ctx->next_change_ms = now + level_ms(ctx->pat->pulses[0].on);
        return 1;
    }

    if (!phase_elapsed(now, *ctx->next_change_ms))
        return -1;

    if (*ctx->phase == LED_PHASE_ON) // 亮段结束 → 转灭
    {
        *ctx->phase = LED_PHASE_OFF;
        *ctx->next_change_ms = now + level_ms(ctx->pat->pulses[*ctx->pulse_idx].off);
        return 0;
    }

    // 灭段结束 → 下一段亮（末段回到首段，形成循环）
    *ctx->pulse_idx = (uint8_t)((*ctx->pulse_idx + 1U) % ctx->pat->count);
    *ctx->phase = LED_PHASE_ON;
    *ctx->next_change_ms = now + level_ms(ctx->pat->pulses[*ctx->pulse_idx].on);
    return 1;
}

// ==================== tLed ====================

bool led_init(tLed *led)
{
    if (!led || !led->ops || !led->handle || !led->ops->open)
        return false;

    // 只重置业务字段；ops / handle 是装配结果，不在本层改动
    led->state = LED_OFF;
    led->pulse_idx = 0U;
    led->phase = LED_PHASE_IDLE;
    led->next_change_ms = 0U;

    return led->ops->open(led->handle);
}

void led_set_state(tLed *led, eLedState state)
{
    if (!led)
        return;
    led->state = state;
    led->pulse_idx = 0U;
    led->phase = LED_PHASE_IDLE; // 强制下个 task 从首段重来
    led->next_change_ms = 0U;
}

void led_task(tLed *led)
{
    if (!led || !led->ops)
        return;

    switch (led->state)
    {
    case LED_ON:
        if (led->ops->set)
            led->ops->set(led->handle, true);
        break;
    case LED_OFF:
        if (led->ops->set)
            led->ops->set(led->handle, false);
        break;
    default:
    {
        const tLedPattern *pat = pattern_of((uint8_t)led->state);
        if (!pat)
            break;

        tBlinkCtx ctx = {
            .pat = pat,
            .pulse_idx = &led->pulse_idx,
            .phase = &led->phase,
            .next_change_ms = &led->next_change_ms,
        };

        int8_t act = blink_step(&ctx);
        if (act >= 0 && led->ops->set)
            led->ops->set(led->handle, act != 0);
        break;
    }
    }
}

// ==================== tRgb ====================

// 亮度推给硬件（颜色由 rgb_set_color 缓存，refresh 一并生效）
static void rgb_apply_brightness(tRgb *rgb, uint8_t value)
{
    if (rgb->ops->set_brightness)
        rgb->ops->set_brightness(rgb->handle, value);
    if (rgb->ops->refresh)
        rgb->ops->refresh(rgb->handle);
}

bool rgb_init(tRgb *rgb)
{
    if (!rgb || !rgb->ops || !rgb->handle || !rgb->ops->open)
        return false;

    // 只重置业务字段；ops / handle 是装配结果，不在本层改动
    rgb->state = RGB_OFF;
    rgb->color = RGB_BLACK;
    rgb->pulse_idx = 0U;
    rgb->phase = LED_PHASE_IDLE;
    rgb->breath_idx = 0U;
    rgb->next_change_ms = 0U;

    return rgb->ops->open(rgb->handle);
}

void rgb_set_state(tRgb *rgb, eRgbState state)
{
    if (!rgb)
        return;
    rgb->state = state;
    rgb->pulse_idx = 0U;
    rgb->phase = LED_PHASE_IDLE; // 强制下个 task 从首段重来
    rgb->breath_idx = 0U;
    rgb->next_change_ms = 0U;
}

void rgb_set_color(tRgb *rgb, tRGBColor color)
{
    if (!rgb || !rgb->ops)
        return;
    rgb->color = color;
    if (rgb->ops->set_color)
        rgb->ops->set_color(rgb->handle, color); // 颜色缓存到驱动，亮度由 task 驱动
}

void rgb_task(tRgb *rgb)
{
    if (!rgb || !rgb->ops)
        return;

    switch (rgb->state)
    {
    case RGB_ON:
        rgb_apply_brightness(rgb, 255U);
        break;
    case RGB_OFF:
        rgb_apply_brightness(rgb, 0U);
        break;
    case RGB_BREATHE:
    {
        uint32_t now = time_get_ms();
        if (!phase_elapsed(now, rgb->next_change_ms))
            break;
        rgb_apply_brightness(rgb, SINE_TABLE[rgb->breath_idx & 63U]);
        rgb->breath_idx = (uint8_t)((rgb->breath_idx + 1U) & 63U);
        rgb->next_change_ms = now + RGB_DEF_BREATHE_MS;
        break;
    }
    default:
    {
        const tLedPattern *pat = pattern_of((uint8_t)rgb->state);
        if (!pat)
            break;

        tBlinkCtx ctx = {
            .pat = pat,
            .pulse_idx = &rgb->pulse_idx,
            .phase = &rgb->phase,
            .next_change_ms = &rgb->next_change_ms,
        };

        int8_t act = blink_step(&ctx);
        if (act >= 0)
            rgb_apply_brightness(rgb, act != 0 ? 255U : 0U);
        break;
    }
    }
}
