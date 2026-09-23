// ============================================================
// sen_mcu.c — 板载 ADC 采样驱动（板级，直连 HAL）
//
//   - ADC2：12bit×3（三相电流，IN3/IN4/IN12），TIM1_CH4 比较触发；采样点可调
//   - ADC1：8bit×2（Vbus/温度，IN1/IN2），TIM2_TRGO 触发
// 采样触发为 PWM mode1 的 CCR 上沿：转换在 PWM 后半周期完成、下溢前就绪，
// 下溢中断触发 FOC 计算时即可用最新电流（转换是硬件行为，不占 CPU）。
// TODO: 电流帧撕裂保护 —— FOC 在下溢中断里直接读 cur_raw，理论上可能读到
//       "半更新"的帧；后续可改 DMA 半满/全满双缓冲。
// ============================================================
#include "sen_mcu.h"

#include "adc.h"
#include "tim.h"

#include "gate_fd6288q.h"

// ---------- 本板配置 ----------
#define SENSE_CUR_CH 3U // 三相电流通道数
#define SENSE_VT_CH 2U  // Vbus + 温度通道数

#define SENSE_TIC_PWM GATE_TIC_PWM // PWM 周期计数值
#define SENSE_CUR_GAIN 100.0f      // 电流采样放大倍数
#define SENSE_VBUS_GAIN 16.0f      // 母线分压比

// ---------- 实例 ----------
struct tSenseAdc
{
    ADC_HandleTypeDef *hadc_cur;    // 外设：三相电流 ADC
    ADC_HandleTypeDef *hadc_vt;     // 外设：Vbus/温度 ADC
    TIM_HandleTypeDef *htim_sample; // 外设：采样点定时器（与功率级同一）
    TIM_HandleTypeDef *htim_vt;     // 外设：触发采样温度电压的定时器
    uint32_t sample_ch;             // 配置：采样点比较通道
    uint32_t tic_default;           // 配置：默认采样点（计数值）
    float cur_scale;                // 配置：电流换算系数
    float vbus_scale;               // 配置：母线电压换算系数

    volatile uint16_t cur_raw[SENSE_CUR_CH]; // DMA 落点：三相电流（设置通道 123-ABC ）
    volatile uint16_t vt_raw[SENSE_VT_CH];   // DMA 落点：Vbus/温度（注意设置好通道 12-vt）

    volatile float current[SENSE_CUR_CH]; // 初步转化电流（未处理零点）
    volatile float vt[SENSE_VT_CH];       // 电压、温度

    volatile bool cur_new, vt_new; // 值更新
};

tSenseAdc g_sen_mcu = {
    .hadc_cur = &hadc2,
    .hadc_vt = &hadc1,
    .htim_sample = &htim1,
    .htim_vt = &htim2,
    .sample_ch = TIM_CHANNEL_4,
    .tic_default = SENSE_TIC_PWM - 1,
    .cur_scale = 3.3f * SENSE_CUR_GAIN / 4095.0f,
    .vbus_scale = 3.3f * SENSE_VBUS_GAIN / 255.0f,
};

// 温度查表：Vbus 归一化后的 NTC 采样码 → ℃
// 表项 adc_eq 按旧驱动公式 adc_eq = code*24/(Vbus-0.3)+0.5 截断取整
// clang-format off
static const uint8_t SENSE_TEMP_TABLE[256] = {
120, 120, 120, 120, 120, 120, 120, 120, 120, 115, 110, 106, 102,  99,  96,  93,
 90,  88,  86,  84,  82,  80,  78,  77,  75,  74,  72,  71,  70,  68,  67,  66,
 65,  64,  63,  62,  61,  60,  59,  58,  57,  57,  56,  55,  54,  54,  53,  52,
 51,  51,  50,  50,  49,  48,  48,  47,  47,  46,  45,  45,  44,  44,  43,  43,
 42,  42,  42,  41,  41,  40,  40,  39,  39,  38,  38,  38,  37,  37,  37,  36,
 36,  35,  35,  35,  34,  34,  34,  33,  33,  33,  32,  32,  32,  31,  31,  31,
 30,  30,  30,  30,  29,  29,  29,  28,  28,  28,  28,  27,  27,  27,  27,  26,
 26,  26,  26,  25,  25,  25,  25,  24,  24,  24,  24,  23,  23,  23,  23,  23,
 22,  22,  22,  22,  21,  21,  21,  21,  21,  20,  20,  20,  20,  20,  19,  19,
 19,  19,  19,  19,  18,  18,  18,  18,  18,  17,  17,  17,  17,  17,  17,  16,
 16,  16,  16,  16,  16,  15,  15,  15,  15,  15,  15,  14,  14,  14,  14,  14,
 14,  14,  13,  13,  13,  13,  13,  13,  12,  12,  12,  12,  12,  12,  12,  11,
 11,  11,  11,  11,  11,  11,  11,  10,  10,  10,  10,  10,  10,  10,   9,   9,
  9,   9,   9,   9,   9,   9,   8,   8,   8,   8,   8,   8,   8,   8,   7,   7,
  7,   7,   7,   7,   7,   7,   7,   6,   6,   6,   6,   6,   6,   6,   6,   6,
  5,   5,   5,   5,   5,   5,   5,   5,   5,   4,   4,   4,   4,   4,   4,   4,};
// clang-format on

// ---- 中断 ----
// HAL 回调由本驱动独占定义（本板两路 ADC 都在本驱动内）。
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc == g_sen_mcu.hadc_cur)
        g_sen_mcu.cur_new = true; // 电流采样完成
    else if (hadc == g_sen_mcu.hadc_vt)
        g_sen_mcu.vt_new = true; // 电压温度采样完成
}

// ---- 驱动接口（tSenseOps） ----

// 必须先启动tim1
static bool sen_mcu_open(void *handle)
{
    tSenseAdc *inst = (tSenseAdc *)handle;
    if (!inst)
        return false;
    if (!inst->hadc_cur || !inst->hadc_vt || !inst->htim_sample || !inst->htim_vt)
        return false;

    HAL_TIM_Base_Start(inst->htim_vt);                                           // 启动触发vt采样的定时器的基础计数器
    HAL_TIM_Base_Start(inst->htim_sample);                                       // 启动触发电流采样的定时器的基础计数器
    __HAL_TIM_SetCompare(inst->htim_sample, inst->sample_ch, inst->tic_default); // 默认采样点

    // 启动 （这里软件触发只是指定buf 后续都是由tim触发）
    if (HAL_ADC_Start_DMA(inst->hadc_cur, (uint32_t *)inst->cur_raw, SENSE_CUR_CH) != HAL_OK)
        return false;

    if (HAL_ADC_Start_DMA(inst->hadc_vt, (uint32_t *)inst->vt_raw, SENSE_VT_CH) != HAL_OK)
        return false;

    return true;
}

static void sen_mcu_set_sample_point(void *handle, uint32_t tic)
{
    const tSenseAdc *inst = (const tSenseAdc *)handle;
    if (!inst)
        return;
    __HAL_TIM_SetCompare(inst->htim_sample, inst->sample_ch, tic);
}

static bool sen_mcu_get_cur(void *handle, float raw[3])
{
    tSenseAdc *inst = (tSenseAdc *)handle;
    if (!inst || !raw)
        return false;

    if (!inst->cur_new) // 未刷新 直接返回
        return true;
    for (uint8_t i = 0U; i < SENSE_CUR_CH; i++)
    {
        uint16_t raw12 = (uint16_t)(inst->cur_raw[i] & 0x0FFFU); // 12bit 右对齐
        inst->current[i] = raw12 * inst->cur_scale;
    }
    return true;
}

static bool sen_mcu_get_vt(void *handle, float *vbus, uint8_t *temp)
{
    tSenseAdc *inst = (tSenseAdc *)handle;
    if (!inst)
        return false;
    if (!inst->vt_new) // 未刷新 直接返回
        return true;
    if (vbus)
        *vbus = (uint16_t)(inst->vt_raw[0] & 0x00FFU) * inst->vbus_scale; // 8bit 右对齐
    if (temp)
        *temp = SENSE_TEMP_TABLE[(uint8_t)(inst->vt_raw[1] & 0x00FFU)];
    return true;
}

// ---- 驱动出口 ----
const tSenseOps sen_mcu_ops = {
    .open = sen_mcu_open,
    .set_sample_point = sen_mcu_set_sample_point,
    .get_cur_raw = sen_mcu_get_cur,
    .get_vt = sen_mcu_get_vt,
};
