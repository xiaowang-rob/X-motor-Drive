// ============================================================
// sense_drv.c — 板采样驱动（usr/drv，v2 直连版）
//
// 实现 usr/abs/sense.h 的 tSampleIf：
//   - ADC1：12bit×3（三相电流），TIM8_TRGO 触发 + DMA 持续
//   - ADC2：8bit×2（Vbus/温度），软件触发，每轮 2 转换
// 中断 HAL_ADC_ConvCpltCallback 由本文件唯一持有（ADC2 完成置新帧）。
//
// 注：ADC 的 GPIO/DMA/中断 MSP 依赖 CubeMX 生成层补全；未就绪时
// init 首帧超时返回 false，上层可见不可用。
// ============================================================

#include "platform.h"
#include "sense_drivers.h"

#define ADC_CUR_CH 3U
#define ADC_VT_CH 2U
#define ADC_INIT_TIMEOUT_MS 100U

static volatile uint16_t s_cur_raw[ADC_CUR_CH]; // DMA HALFWORD 连续写（半字对齐）
static volatile uint16_t s_vt_raw[ADC_VT_CH];
static volatile bool s_vt_new = false;

// ---- 中断（只在本文件定义） ----
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC2)
        s_vt_new = true;
}

// ---- tSampleMcuOps 实现 ----

static bool sd_init(void *ctx)
{
    (void)ctx;
    __HAL_TIM_SetCompare(&PWM_GET_HTIM, TIM_CHANNEL_4, TIC_PWM - 1U); // 默认采样点

    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_cur_raw, ADC_CUR_CH) != HAL_OK)
        return false;

    s_vt_new = false;
    if (HAL_ADC_Start_DMA(&hadc2, (uint32_t *)s_vt_raw, ADC_VT_CH) != HAL_OK)
        return false;

    uint32_t t0 = platform_get_ms();
    while (!s_vt_new && (platform_get_ms() - t0) < ADC_INIT_TIMEOUT_MS)
    {
    }
    return s_vt_new;
}

static void sd_set_sample_cmp(void *ctx, uint32_t tic)
{
    (void)ctx;
    __HAL_TIM_SetCompare(&SAMPLE_PWM_HTIM, SAMPLE_PWM_CHANNEL, tic);
}

static bool sd_get_cur_raw(void *ctx, uint16_t raw[3])
{
    (void)ctx;
    for (uint8_t i = 0U; i < ADC_CUR_CH; i++)
        raw[i] = (uint16_t)(s_cur_raw[i] & 0x0FFFU);
    return true;
}

static void sd_vt_trigger(void *ctx)
{
    (void)ctx;
    if (HAL_ADC_GetState(&hadc2) != HAL_ADC_STATE_READY)
        return;
    s_vt_new = false;
    HAL_ADC_Start_DMA(&hadc2, (uint32_t *)s_vt_raw, ADC_VT_CH);
}

static bool sd_get_vt_raw(void *ctx, uint16_t *vbus_raw, uint16_t *temp_raw)
{
    (void)ctx;
    if (!s_vt_new)
        return false;
    s_vt_new = false;
    if (vbus_raw)
        *vbus_raw = (uint16_t)(s_vt_raw[0] & 0x00FFU);
    if (temp_raw)
        *temp_raw = (uint16_t)(s_vt_raw[1] & 0x00FFU);
    return true;
}

static void sd_get_gain(void *ctx, float *cur_scale, float *vbus_scale)
{
    (void)ctx;
    if (cur_scale)
        *cur_scale = 3.3f * RATE_CURRENT_SAMPLE / 4095.0f;
    if (vbus_scale)
        *vbus_scale = 3.3f * (float)RATE_VOLTAGE_SAMPLE / 255.0f;
}

const tSampleMcuOps mcu_adc_ops = {
    .ctx = NULL,
    .init = sd_init,
    .set_sample_cmp = sd_set_sample_cmp,
    .get_cur_raw = sd_get_cur_raw,
    .vt_trigger = sd_vt_trigger,
    .get_vt_raw = sd_get_vt_raw,
    .get_gain = sd_get_gain,
};
