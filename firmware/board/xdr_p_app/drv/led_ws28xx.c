// ============================================================
// led_ws28xx.c — WS2812/WS28xx RGB 灯驱动（板级，PWM + DMA）
//
// 芯片协议：单线串行、每灯 24bit（GRB），每位由高低占空比定时脉冲表达，
// 整体由 PWM+DMA 推 CCR 值序列产生。
//
// 实现 abs/led.h 的 tRgbOps。
// 实例形态：外部链接实例（外设 / 配置 / 静态 CCR 缓冲），无堆。
// 中断：HAL_TIM_PWM_PulseFinishedCallback 本体在 bsp_irq，本驱动只注册处理函数。
// ============================================================
#include "led_ws28xx.h"

#include "bsp_irq.h"

#include "tim.h"

// ---------- 本板配置 ----------
#define RGB_PWM_CHANNEL TIM_CHANNEL_2
#define RGB_PIXEL_NUM 2U    // 板上灯珠数量
#define RGB_CODE_1 75U      // 逻辑 1 的 CCR 值
#define RGB_CODE_0 35U      // 逻辑 0 的 CCR 值
#define RGB_RESET_BITS 100U // 帧尾复位（低电平）位数

#define RGB_BUF_LEN (RGB_PIXEL_NUM * 24U + RGB_RESET_BITS)

// ---------- 实例 ----------
struct tWs28xx
{
    TIM_HandleTypeDef *htim; // 外设：PWM 定时器
    uint32_t channel;        // 配置：PWM 通道
    uint8_t num_pixels;      // 配置：灯珠数量
    uint32_t buf_len;        // 配置：CCR 序列长度（含帧尾复位位）
    uint16_t code_1;         // 配置：逻辑 1 占空比
    uint16_t code_0;         // 配置：逻辑 0 占空比

    tRGBColor color;               // 运行时：颜色
    uint8_t brightness;            // 运行时：亮度
    bool inited;                   // 运行时：初始化标志
    volatile bool busy;            // 运行时：DMA 推流中
    uint32_t ccr_buf[RGB_BUF_LEN]; // 静态 DMA 缓冲
};

tWs28xx g_ws28xx = {
    .htim = &htim4,
    .channel = RGB_PWM_CHANNEL,
    .num_pixels = RGB_PIXEL_NUM,
    .buf_len = RGB_BUF_LEN,
    .code_1 = RGB_CODE_1,
    .code_0 = RGB_CODE_0,
    .brightness = 255U,
    .inited = false,
    .busy = false,
};

// ---- 传输完成中断 ----
// HAL 回调本体在 bsp_irq；本驱动只注册处理函数（见 ws28xx_open）。
static void ws28xx_on_pulse_done(void *ctx)
{
    tWs28xx *inst = (tWs28xx *)ctx;
    if (inst)
        inst->busy = false;
}

// 亮度缩放（0-255）
static inline uint8_t scale_brightness(uint8_t v, uint8_t b)
{
    return (uint8_t)(((uint16_t)v * b + 127U) / 255U);
}

// 颜色 + 亮度 → CCR 序列
static void ws28xx_build_stream(tWs28xx *inst)
{
    uint32_t idx = 0U;
    for (uint8_t pixel = 0U; pixel < inst->num_pixels; pixel++)
    {
        uint8_t ch[3]; // G, R, B（WS2812 数据格式）
        ch[0] = scale_brightness(inst->color.G, inst->brightness);
        ch[1] = scale_brightness(inst->color.R, inst->brightness);
        ch[2] = scale_brightness(inst->color.B, inst->brightness);

        for (uint8_t c = 0U; c < 3U; c++)
            for (int8_t bit = 7; bit >= 0; bit--)
                inst->ccr_buf[idx++] = ((ch[c] >> bit) & 0x01U) ? inst->code_1 : inst->code_0;
    }
    for (uint32_t i = idx; i < inst->buf_len; i++)
        inst->ccr_buf[i] = 0U; // 帧尾复位（低电平）
}

// ---- 驱动接口（tRgbOps） ----

static bool ws28xx_open(void *handle)
{
    tWs28xx *inst = (tWs28xx *)handle;
    if (!inst || !inst->htim)
        return false;

    // 注册"推送完成"处理函数，用于复位忙标志
    static const tTimIrq s_rgb_irq = {
        .on_overflow = NULL,
        .on_underflow = NULL,
        .on_pulse_done = ws28xx_on_pulse_done,
        .ctx = &g_ws28xx,
    };
    if (!bsp_irq_bind_tim(inst->htim->Instance, &s_rgb_irq))
        return false;

    memset(inst->ccr_buf, 0, inst->buf_len * sizeof(uint32_t));
    inst->brightness = 255U;
    inst->color.R = inst->color.G = inst->color.B = 0U;
    inst->busy = false;
    inst->inited = true;
    return true;
}

static void ws28xx_set_color(void *handle, tRGBColor color)
{
    tWs28xx *inst = (tWs28xx *)handle;
    if (!inst)
        return;
    inst->color = color;
}

static void ws28xx_set_brightness(void *handle, uint8_t brightness)
{
    tWs28xx *inst = (tWs28xx *)handle;
    if (!inst)
        return;
    inst->brightness = brightness;
}

static void ws28xx_refresh(void *handle)
{
    tWs28xx *inst = (tWs28xx *)handle;
    if (!inst || !inst->inited)
        return;
    if (inst->busy)
        return; // 上一帧仍在推，丢弃本次

    ws28xx_build_stream(inst);
    inst->busy = true;
    if (HAL_TIM_PWM_Start_DMA(inst->htim, inst->channel,
                              inst->ccr_buf, (uint16_t)inst->buf_len) != HAL_OK)
        inst->busy = false;
}

// ---- 驱动出口 ----
const tRgbOps ws28xx_ops = {
    .open = ws28xx_open,
    .set_color = ws28xx_set_color,
    .set_brightness = ws28xx_set_brightness,
    .refresh = ws28xx_refresh,
};
