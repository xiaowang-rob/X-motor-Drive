// ============================================================
// ws28xx.c — WS2812/WS28xx RGB 灯驱动（usr/drv，v2 直连版）
//
// 芯片协议：单线串行、每灯 24bit（GRB）、每位由高低占空比定时脉冲表达，
// 整体由 PWM+DMA 推 CCR 值序列产生。驱动负责 颜色+亮度 → CCR 编码。
//
// v2：直接用本板 PWM-DMA（platform.h 的 RGB_PWM_GET_HTIM/CH1），
// 灯珠数取 Pixel_NUM；传输完成中断 HAL_TIM_PWM_PulseFinishedCallback
// 由本文件唯一持有（复位忙标志）。
// ============================================================

#include "led.h"

#include "platform.h"
#include "rgb_drivers.h"

#define WS_RESET_BITS 100U // 帧尾复位（低电平）位数

static volatile bool s_busy = false; // DMA 推流中

// ---- 传输完成中断（只在本文件定义） ----
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == RGB_PWM_GET_HTIM.Instance)
        s_busy = false;
}

typedef struct
{
    uint8_t num_pixels;
    uint32_t buf_len; // num*24 + reset
    tRGBColor color;
    uint8_t brightness;
    bool inited;
    uint32_t *ccr_buf;
} tWs28xx_ctx;

// 改变亮度时，亮度值范围 0-255，颜色值范围 0-255
static inline uint8_t scale_brightness(uint8_t v, uint8_t b)
{
    return (uint8_t)(((uint16_t)v * b + 127U) / 255U);
}
// RGB 编码
static void ws28xx_build_stream(tWs28xx_ctx *ctx)
{
    uint32_t idx = 0U;
    for (uint8_t pixel = 0U; pixel < ctx->num_pixels; pixel++)
    {
        uint8_t ch[3]; // G,R,B（WS2812 数据格式）
        ch[0] = scale_brightness(ctx->color.G, ctx->brightness);
        ch[1] = scale_brightness(ctx->color.R, ctx->brightness);
        ch[2] = scale_brightness(ctx->color.B, ctx->brightness);

        for (uint8_t c = 0U; c < 3U; c++)
            for (int bit = 7; bit >= 0; bit--)
                ctx->ccr_buf[idx++] = ((ch[c] >> bit) & 0x01U) ? CODE_1 : CODE_0;
    }
    for (uint32_t i = idx; i < ctx->buf_len; i++)
        ctx->ccr_buf[i] = 0U; // 复位（低电平）
}

// ---- ops 实现 ----

static bool ws28xx_init(RgbHandle h)
{
    tWs28xx_ctx *ctx = (tWs28xx_ctx *)h;
    if (!ctx)
        return false;

    memset(ctx->ccr_buf, 0, ctx->buf_len * sizeof(uint32_t));
    ctx->brightness = 255U;
    ctx->color.R = ctx->color.G = ctx->color.B = 0U;
    ctx->inited = true;
    return true;
}

static void ws28xx_set_rgb(RgbHandle h, tRGBColor color)
{
    tWs28xx_ctx *ctx = (tWs28xx_ctx *)h;
    if (!ctx)
        return;
    ctx->color = color;
}

static void ws28xx_set_brightness(RgbHandle h, uint8_t brightness)
{
    tWs28xx_ctx *ctx = (tWs28xx_ctx *)h;
    if (!ctx)
        return;
    ctx->brightness = brightness;
}

static void ws28xx_refresh(RgbHandle h)
{
    tWs28xx_ctx *ctx = (tWs28xx_ctx *)h;
    if (!ctx || !ctx->inited)
        return;

    if (s_busy)
        return; // 上一帧仍在推，丢弃本次

    ws28xx_build_stream(ctx);
    s_busy = true;
    if (HAL_TIM_PWM_Start_DMA(&RGB_PWM_GET_HTIM, RGB_PWM_CHANNEL1,
                              ctx->ccr_buf, (uint16_t)ctx->buf_len) != HAL_OK)
        s_busy = false;
}

const tRgbDriverOps ws28xx_driver_ops = {
    .init = ws28xx_init,
    .set_rgb = ws28xx_set_rgb,
    .set_brightness = ws28xx_set_brightness,
    .refresh = ws28xx_refresh,
};

RgbHandle ws28xx_create(void)
{
    uint8_t num_pixels = Pixel_NUM; // 板上灯珠数（platform）
    if (num_pixels == 0U)
        return NULL;

    tWs28xx_ctx *ctx = (tWs28xx_ctx *)calloc(1U, sizeof(tWs28xx_ctx));
    if (!ctx)
        return NULL;

    ctx->buf_len = (uint32_t)num_pixels * 24U + WS_RESET_BITS;
    ctx->ccr_buf = (uint32_t *)calloc(ctx->buf_len, sizeof(uint32_t));
    if (!ctx->ccr_buf)
    {
        free(ctx);
        return NULL;
    }

    ctx->num_pixels = num_pixels;
    ctx->brightness = 255U;
    return (RgbHandle)ctx;
}

void ws28xx_destroy(RgbHandle h)
{
    tWs28xx_ctx *ctx = (tWs28xx_ctx *)h;
    if (!ctx)
        return;
    free(ctx->ccr_buf);
    free(ctx);
    h = NULL;
}
