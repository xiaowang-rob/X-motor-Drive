// ============================================================
// sen_mcu.c — 板采样驱动（板级，直连 HAL）
//
// 实现 abs 的 tSampleMcuOps：
//   - ADC1：12bit×3（三相电流），TIM8_TRGO 触发 + DMA 持续
//   - ADC2：8bit×2（Vbus/温度），软件触发，每轮 2 转换
// 实例形态：文件内静态 handle，内含
//   - ops 指针、外设句柄（hadc_cur / hadc_vt / htim_sample）
//   - 配置（采样点通道与默认值、换算倍数）
//   - DMA 落点缓冲与状态标志
// HAL_ADC_ConvCpltCallback 由本文件唯一持有（ADC2 完成置新帧标志）。
//
// TODO: 电流帧撕裂保护 —— ADC1 走循环 DMA 持续写 cur_raw，而 FOC 在下溢
//       中断里直接读取，理论上可能读到"半更新"的帧。后续可改为 DMA
//       半满/全满双缓冲，或在采样点之后读取并做一致性判断。
// ============================================================
#include "sense_drivers.h"

#include "adc.h"
#include "tim.h"

// ---------- 本板配置 ----------
#define SENSE_CUR_CH 3U // 三相电流通道数
#define SENSE_VT_CH 2U  // Vbus + 温度通道数
#define SENSE_INIT_TIMEOUT_MS 100U

#define SENSE_TIC_PWM 2099U   // PWM 周期计数值（须与栅极驱动的 PWM 周期一致）
#define SENSE_CUR_GAIN 100.0f // 电流采样放大倍数
#define SENSE_VBUS_GAIN 16.0f // 母线分压比

// ---------- 实例 handle ----------
typedef struct
{
    ADC_HandleTypeDef *hadc_cur;    // 外设：三相电流 ADC
    ADC_HandleTypeDef *hadc_vt;     // 外设：Vbus/温度 ADC
    TIM_HandleTypeDef *htim_sample; // 外设：采样点定时器（与功率级同一 TIM8）
    uint32_t sample_ch;             // 配置：采样点比较通道
    uint32_t tic_default;           // 配置：默认采样点（计数值）
    float cur_gain;                 // 配置：电流换算倍数
    float vbus_gain;                // 配置：母线换算倍数

    volatile uint16_t cur_raw[SENSE_CUR_CH]; // DMA 落点：三相电流
    volatile uint16_t vt_raw[SENSE_VT_CH];   // DMA 落点：Vbus/温度
    volatile bool vt_new;                    // Vbus/温度新帧标志
} tSenseAdc;

static bool sd_init(SampleHandle h);
static void sd_set_sample_cmp(SampleHandle h, uint32_t tic);
static bool sd_get_cur_raw(SampleHandle h, uint16_t raw[3]);
static void sd_vt_trigger(SampleHandle h);
static bool sd_get_vt_raw(SampleHandle h, uint16_t *vbus_raw, uint16_t *temp_raw);
static void sd_get_gain(SampleHandle h, float *cur_scale, float *vbus_scale);

const tSampleMcuOps mcu_adc_ops = {
    .init = sd_init,
    .set_sample_cmp = sd_set_sample_cmp,
    .get_cur_raw = sd_get_cur_raw,
    .vt_trigger = sd_vt_trigger,
    .get_vt_raw = sd_get_vt_raw,
    .get_gain = sd_get_gain,
};

// 静态实例
static tSenseAdc s_adc = {
    .hadc_cur = &hadc1,
    .hadc_vt = &hadc2,
    .htim_sample = &htim8,
    .sample_ch = TIM_CHANNEL_4,
    .tic_default = SENSE_TIC_PWM,
    .cur_gain = SENSE_CUR_GAIN,
    .vbus_gain = SENSE_VBUS_GAIN,
    .vt_new = false,
};

SampleHandle sense_get_handle(void)
{
    return (SampleHandle)&s_adc;
}

// ---- 中断（只在本文件定义） ----
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == s_adc.hadc_vt->Instance)
        s_adc.vt_new = true;
}

// ---- ops 实现 ----

static bool sd_init(SampleHandle h)
{
    tSenseAdc *inst = (tSenseAdc *)h;
    if (!inst || !inst->hadc_cur || !inst->hadc_vt || !inst->htim_sample)
        return false;

    __HAL_TIM_SetCompare(inst->htim_sample, inst->sample_ch, inst->tic_default - 1U); // 默认采样点

    if (HAL_ADC_Start_DMA(inst->hadc_cur, (uint32_t *)inst->cur_raw, SENSE_CUR_CH) != HAL_OK)
        return false;

    inst->vt_new = false;
    if (HAL_ADC_Start_DMA(inst->hadc_vt, (uint32_t *)inst->vt_raw, SENSE_VT_CH) != HAL_OK)
        return false;

    // 首帧 Vbus/温度转换完成即视为可用（软件触发需一次启动）
    uint32_t t0 = HAL_GetTick();
    while (!inst->vt_new && (HAL_GetTick() - t0) < SENSE_INIT_TIMEOUT_MS)
    {
    }
    return inst->vt_new;
}

static void sd_set_sample_cmp(SampleHandle h, uint32_t tic)
{
    tSenseAdc *inst = (tSenseAdc *)h;
    if (!inst || !inst->htim_sample)
        return;
    __HAL_TIM_SetCompare(inst->htim_sample, inst->sample_ch, tic);
}

static bool sd_get_cur_raw(SampleHandle h, uint16_t raw[3])
{
    tSenseAdc *inst = (tSenseAdc *)h;
    if (!inst || !raw)
        return false;
    for (uint8_t i = 0U; i < SENSE_CUR_CH; i++)
        raw[i] = (uint16_t)(inst->cur_raw[i] & 0x0FFFU); // 12bit 右对齐
    return true;
}

static void sd_vt_trigger(SampleHandle h)
{
    tSenseAdc *inst = (tSenseAdc *)h;
    if (!inst || !inst->hadc_vt)
        return;
    if (HAL_ADC_GetState(inst->hadc_vt) != HAL_ADC_STATE_READY)
        return;
    inst->vt_new = false;
    HAL_ADC_Start_DMA(inst->hadc_vt, (uint32_t *)inst->vt_raw, SENSE_VT_CH);
}

static bool sd_get_vt_raw(SampleHandle h, uint16_t *vbus_raw, uint16_t *temp_raw)
{
    tSenseAdc *inst = (tSenseAdc *)h;
    if (!inst)
        return false;
    if (!inst->vt_new)
        return false;
    inst->vt_new = false;
    if (vbus_raw)
        *vbus_raw = (uint16_t)(inst->vt_raw[0] & 0x00FFU); // 8bit 右对齐
    if (temp_raw)
        *temp_raw = (uint16_t)(inst->vt_raw[1] & 0x00FFU);
    return true;
}

static void sd_get_gain(SampleHandle h, float *cur_scale, float *vbus_scale)
{
    tSenseAdc *inst = (tSenseAdc *)h;
    if (!inst)
        return;
    if (cur_scale)
        *cur_scale = 3.3f * inst->cur_gain / 4095.0f;
    if (vbus_scale)
        *vbus_scale = 3.3f * inst->vbus_gain / 255.0f;
}
