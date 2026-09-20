// ============================================================
// sen_mcu.c — 板采样驱动（板级，直连 HAL）
//
// 实现 abs/sense.h 的 tSenseOps：
//   - ADC1：12bit×3（三相电流），TIM8_TRGO 触发 + DMA 持续
//   - ADC2：8bit×2（Vbus/温度），软件触发，每轮 2 转换
// 实例形态：外部链接实例（外设 / 配置 / DMA 落点缓冲），无堆。
// 中断：HAL_ADC_ConvCpltCallback 本体在 bsp_irq，本驱动只注册处理函数。
//
// TODO: 电流帧撕裂保护 —— ADC1 走循环 DMA 持续写 cur_raw，而 FOC 在下溢
//       中断里直接读取，理论上可能读到"半更新"的帧。后续可改为 DMA
//       半满/全满双缓冲，或在采样点之后读取并做一致性判断。
// ============================================================
#include "sen_mcu.h"

#include "bsp_irq.h"
#include "bsp_time.h"

#include "adc.h"
#include "tim.h"

// ---------- 本板配置 ----------
#define SENSE_CUR_CH 3U // 三相电流通道数
#define SENSE_VT_CH 2U  // Vbus + 温度通道数
#define SENSE_INIT_TIMEOUT_MS 100U

#define SENSE_TIC_PWM 2099U   // PWM 周期计数值（须与栅极驱动的 PWM 周期一致）
#define SENSE_CUR_GAIN 100.0f // 电流采样放大倍数
#define SENSE_VBUS_GAIN 16.0f // 母线分压比

// ---------- 实例 ----------
struct tSenseAdc
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
};

tSenseAdc g_sen_mcu = {
    .hadc_cur = &hadc1,
    .hadc_vt = &hadc2,
    .htim_sample = &htim8,
    .sample_ch = TIM_CHANNEL_4,
    .tic_default = SENSE_TIC_PWM,
    .cur_gain = SENSE_CUR_GAIN,
    .vbus_gain = SENSE_VBUS_GAIN,
    .vt_new = false,
};

// ---- 中断 ----
// HAL 回调本体在 bsp_irq；本驱动只注册处理函数（见 sen_mcu_open）。
static void sd_on_vt_cplt(void *ctx)
{
    tSenseAdc *inst = (tSenseAdc *)ctx;
    if (inst)
        inst->vt_new = true;
}

// ---- 驱动接口（tSenseOps） ----

static bool sen_mcu_open(void *handle, float *cur_scale, float *vbus_scale)
{
    tSenseAdc *inst = (tSenseAdc *)handle;
    if (!inst || !cur_scale || !vbus_scale)
        return false;
    if (!inst->hadc_cur || !inst->hadc_vt || !inst->htim_sample)
        return false;

    // 注册 Vbus/温度转换完成处理函数（ADC1 走循环 DMA，无需完成回调）
    static const tAdcIrq s_vt_irq = {
        .on_conv_cplt = sd_on_vt_cplt,
        .ctx = &g_sen_mcu,
    };
    if (!bsp_irq_bind_adc(inst->hadc_vt->Instance, &s_vt_irq))
        return false;

    __HAL_TIM_SetCompare(inst->htim_sample, inst->sample_ch, inst->tic_default - 1U); // 默认采样点

    if (HAL_ADC_Start_DMA(inst->hadc_cur, (uint32_t *)inst->cur_raw, SENSE_CUR_CH) != HAL_OK)
        return false;

    inst->vt_new = false;
    if (HAL_ADC_Start_DMA(inst->hadc_vt, (uint32_t *)inst->vt_raw, SENSE_VT_CH) != HAL_OK)
        return false;

    // 首帧 Vbus/温度转换完成即视为可用（软件触发需一次启动）
    uint32_t t0 = bsp_time_ms();
    while (!inst->vt_new && (bsp_time_ms() - t0) < SENSE_INIT_TIMEOUT_MS)
    {
    }
    if (!inst->vt_new)
        return false;

    // 换算系数（码 → 物理量）
    *cur_scale = 3.3f * inst->cur_gain / 4095.0f;
    *vbus_scale = 3.3f * inst->vbus_gain / 255.0f;
    return true;
}

static void sen_mcu_set_sample_point(void *handle, uint32_t tic)
{
    const tSenseAdc *inst = (const tSenseAdc *)handle;
    if (!inst)
        return;
    __HAL_TIM_SetCompare(inst->htim_sample, inst->sample_ch, tic);
}

static bool sen_mcu_get_cur_raw(void *handle, uint16_t raw[3])
{
    const tSenseAdc *inst = (const tSenseAdc *)handle;
    if (!inst || !raw)
        return false;
    for (uint8_t i = 0U; i < SENSE_CUR_CH; i++)
        raw[i] = (uint16_t)(inst->cur_raw[i] & 0x0FFFU); // 12bit 右对齐
    return true;
}

static void sen_mcu_vt_trigger(void *handle)
{
    tSenseAdc *inst = (tSenseAdc *)handle;
    if (!inst)
        return;
    if (HAL_ADC_GetState(inst->hadc_vt) != HAL_ADC_STATE_READY)
        return;
    inst->vt_new = false;
    HAL_ADC_Start_DMA(inst->hadc_vt, (uint32_t *)inst->vt_raw, SENSE_VT_CH);
}

static bool sen_mcu_get_vt_raw(void *handle, uint16_t *vbus_raw, uint16_t *temp_raw)
{
    tSenseAdc *inst = (tSenseAdc *)handle;
    if (!inst || !inst->vt_new)
        return false;
    inst->vt_new = false;
    if (vbus_raw)
        *vbus_raw = (uint16_t)(inst->vt_raw[0] & 0x00FFU); // 8bit 右对齐
    if (temp_raw)
        *temp_raw = (uint16_t)(inst->vt_raw[1] & 0x00FFU);
    return true;
}

// ---- 驱动出口 ----
const tSenseOps sen_mcu_ops = {
    .open = sen_mcu_open,
    .set_sample_point = sen_mcu_set_sample_point,
    .get_cur_raw = sen_mcu_get_cur_raw,
    .vt_trigger = sen_mcu_vt_trigger,
    .get_vt_raw = sen_mcu_get_vt_raw,
};
