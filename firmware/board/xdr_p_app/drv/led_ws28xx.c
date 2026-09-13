// ============================================================
// led_ws28xx.c — WS2812/WS28xx RGB 灯驱动（板级，PWM + DMA）
//
// 芯片协议：单线串行、每灯 24bit（GRB），每位由高低占空比定时脉冲表达，
// 整体由 PWM+DMA 推 CCR 值序列产生。
// 实例形态：文件内静态 handle，内含
//   - ops 指针、外设句柄（htim）、配置（通道 / 灯珠数 / 占空比编码 / 缓冲长度）
//   - 静态 CCR 缓冲（不再堆分配）
// HAL_TIM_PWM_PulseFinishedCallback 由本文件唯一持有（复位忙标志）。
// ============================================================
#include "led_drivers.h"

#include "tim.h"

// ---------- 本板配置 ----------
#define RGB_PWM_CHANNEL TIM_CHANNEL_2
#define RGB_PIXEL_NUM 2U    // 板上灯珠数量
#define RGB_CODE_1 75U      // 逻辑 1 的 CCR 值
#define RGB_CODE_0 35U      // 逻辑 0 的 CCR 值
#define RGB_RESET_BITS 100U // 帧尾复位（低电平）位数

#define RGB_BUF_LEN (RGB_PIXEL_NUM * 24U + RGB_RESET_BITS)

// ---------- 实例 handle ----------
typedef struct
{
    const tRgbDriverOps *ops; // 该实例的操作表
    TIM_HandleTypeDef *htim;  // 外设：PWM 定时器
    uint32_t channel;         // 配置：PWM 通道
    uint8_t num_pixels;       // 配置：灯珠数量
    uint32_t buf_len;         // 配置：CCR 序列长度（含帧尾复位位）
    uint16_t code_1;          // 配置：逻辑 1 占空比
    uint16_t code_0;          // 配置：逻辑 0 占空比

    tRGBColor color;               // 运行时：颜色
    uint8_t brightness;            // 运行时：亮度
    bool inited;                   // 运行时：初始化标志
    volatile bool busy;            // 运行时：DMA 推流中
    uint32_t ccr_buf[RGB_BUF_LEN]; // 静态 DMA 缓冲
} tWs28xx;

static bool ws28xx_init(RgbHandle h);
static void ws28xx_set_rgb(RgbHandle h, tRGBColor color);
static void ws28xx_set_brightness(RgbHandle h, uint8_t brightness);
static void ws28xx_refresh(RgbHandle h);

const tRgbDriverOps rgb_ws28xx_ops = {
    .init = ws28xx_init,
    .set_rgb = ws28xx_set_rgb,
    .set_brightness = ws28xx_set_brightness,
    .refresh = ws28xx_refresh,
};

// 静态实例
static tWs28xx s_rgb = {
    .ops = &rgb_ws28xx_ops,
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

RgbHandle rgb_get_handle(void)
{
    return (RgbHandle)&s_rgb;
}

// ---- 传输完成中断（只在本文件定义） ----
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if (htim != s_rgb.htim)
        return;
    s_rgb.busy = false;
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

// ---- ops 实现 ----

static bool ws28xx_init(RgbHandle h)
{
    tWs28xx *inst = (tWs28xx *)h;
    if (!inst || !inst->htim)
        return false;

    memset(inst->ccr_buf, 0, inst->buf_len * sizeof(uint32_t));
    inst->brightness = 255U;
    inst->color.R = inst->color.G = inst->color.B = 0U;
    inst->busy = false;
    inst->inited = true;
    return true;
}

static void ws28xx_set_rgb(RgbHandle h, tRGBColor color)
{
    tWs28xx *inst = (tWs28xx *)h;
    if (!inst)
        return;
    inst->color = color;
}

static void ws28xx_set_brightness(RgbHandle h, uint8_t brightness)
{
    tWs28xx *inst = (tWs28xx *)h;
    if (!inst)
        return;
    inst->brightness = brightness;
}

static void ws28xx_refresh(RgbHandle h)
{
    tWs28xx *inst = (tWs28xx *)h;
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
