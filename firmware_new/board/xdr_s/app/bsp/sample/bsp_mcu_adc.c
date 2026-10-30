// MCU 普通采样：ADC1/TIM1 采集三相电流，ADC2/TIM2 采集母线电压和温度。
#include "bsp_adc.h"

static const uint8_t TEMP_TABLE[256] = {
    120, 120, 120, 120, 120, 120, 120, 120, 120, 115, 110, 106, 102, 99, 96, 93,
    90,  88,  86,  84,  82,  80,  78,  77,  75,  74,  72,  71,  70,  68, 67, 66,
    65,  64,  63,  62,  61,  60,  59,  58,  57,  57,  56,  55,  54,  54, 53, 52,
    51,  51,  50,  50,  49,  48,  48,  47,  47,  46,  45,  45,  44,  44, 43, 43,
    42,  42,  42,  41,  41,  40,  40,  39,  39,  38,  38,  38,  37,  37, 37, 36,
    36,  35,  35,  35,  34,  34,  34,  33,  33,  33,  32,  32,  32,  31, 31, 31,
    30,  30,  30,  30,  29,  29,  29,  28,  28,  28,  28,  27,  27,  27, 27, 26,
    26,  26,  26,  25,  25,  25,  25,  24,  24,  24,  24,  24,  23,  23, 23, 23,
    22,  22,  22,  22,  22,  21,  21,  21,  21,  21,  20,  20,  20,  20, 20, 19,
    19,  19,  19,  19,  18,  18,  18,  18,  18,  17,  17,  17,  17,  17, 17, 16,
    16,  16,  16,  16,  16,  15,  15,  15,  15,  15,  15,  14,  14,  14, 14, 14,
    14,  14,  13,  13,  13,  13,  13,  13,  12,  12,  12,  12,  12,  12, 12, 11,
    11,  11,  11,  11,  11,  11,  11,  10,  10,  10,  10,  10,  10,  10, 9,  9,
    9,   9,   9,   9,   9,   9,   8,   8,   8,   8,   8,   8,   8,   8,  8,  7,
    7,   7,   7,   7,   7,   7,   7,   6,   6,   6,   6,   6,   6,   6,  6,  6,
    5,   5,   5,   5,   5,   5,   5,   5,   5,   4,   4,   4,   4,   4,  4,  4,
};
typedef struct MCU_ADC_DEVICE {
    volatile uint16_t cur_adc_buf[CUR_ADC_SEQ_LEN];
    volatile uint16_t vt_adc_buf[VT_ADC_SEQ_LEN];
} tMcuAdcDev;
static tMcuAdcDev mcu_adc;

static const float current_scale = 3.3f * SENSE_CUR_GAIN / 4095.0f;
static const float vbus_scale = 3.3f * SENSE_VBUS_GAIN / 255.0f;

// 校准时必须 ADEN=0；ADC 内部稳压器已由 CubeMX 初始化并等待稳定。
static bool adc_enable(ADC_TypeDef *adc) {
    if (LL_ADC_IsEnabled(adc))
        return false;
    LL_ADC_StartCalibration(adc, LL_ADC_SINGLE_ENDED);
    uint32_t timeout = 1000000U;
    while (LL_ADC_IsCalibrationOnGoing(adc))
        if (--timeout == 0U)
            return false;
    for (volatile unsigned i = 0; i < 100U; ++i)
        __NOP();
    LL_ADC_ClearFlag_ADRDY(adc);
    LL_ADC_ClearFlag_OVR(adc);
    LL_ADC_Enable(adc);
    timeout = 1000000U;
    while (!LL_ADC_IsActiveFlag_ADRDY(adc))
        if (--timeout == 0U)
            return false;
    return true;
}

static bool vt_open(volatile uint16_t *adc_buf) {
    // ADC2 的 8 位分辨率、扫描通道和 TIM2 触发配置均沿用 CubeMX。
    if (!adc_enable(VT_ADC))
        return false;
    LL_DMA_DisableChannel(VT_ADC_DMA, VT_ADC_DMA_CH);
    LL_DMA_SetMode(VT_ADC_DMA, VT_ADC_DMA_CH, LL_DMA_MODE_CIRCULAR);
    LL_DMA_SetPeriphAddress(
        VT_ADC_DMA,
        VT_ADC_DMA_CH,
        LL_ADC_DMA_GetRegAddr(VT_ADC, LL_ADC_DMA_REG_REGULAR_DATA));
    LL_DMA_SetMemoryAddress(VT_ADC_DMA, VT_ADC_DMA_CH, (uint32_t)adc_buf);
    LL_DMA_SetDataLength(VT_ADC_DMA, VT_ADC_DMA_CH, VT_ADC_SEQ_LEN);
    LL_DMA_ClearFlag_GI4(VT_ADC_DMA);
    LL_DMA_EnableChannel(VT_ADC_DMA, VT_ADC_DMA_CH);
    LL_ADC_REG_StartConversion(VT_ADC);
    LL_TIM_EnableCounter(VT_ADC_TIM);
    return true;
}

static bool current_open(volatile uint16_t *adc_buf) {
    // ADC1 的三通道序列、TIM1 TRGO 和过采样配置沿用 CubeMX。
    if (!adc_enable(CUR_ADC))
        return false;
    LL_DMA_DisableChannel(CUR_ADC_DMA, CUR_ADC_DMA_CH);
    LL_DMA_SetMode(CUR_ADC_DMA, CUR_ADC_DMA_CH, LL_DMA_MODE_CIRCULAR);
    LL_DMA_SetPeriphAddress(
        CUR_ADC_DMA,
        CUR_ADC_DMA_CH,
        LL_ADC_DMA_GetRegAddr(CUR_ADC, LL_ADC_DMA_REG_REGULAR_DATA));
    LL_DMA_SetMemoryAddress(CUR_ADC_DMA, CUR_ADC_DMA_CH, (uint32_t)adc_buf);
    LL_DMA_SetDataLength(CUR_ADC_DMA, CUR_ADC_DMA_CH, CUR_ADC_SEQ_LEN);
    LL_DMA_ClearFlag_GI1(CUR_ADC_DMA);
    LL_DMA_EnableChannel(CUR_ADC_DMA, CUR_ADC_DMA_CH);
    CUR_SET_DEF_SP();
    LL_ADC_REG_StartConversion(CUR_ADC);
    LL_TIM_EnableCounter(CUR_ADC_TIM);
    return true;
}

static bool sample_open(void *handle) {
    if (!handle)
        return false;
    tMcuAdcDev *dev = (tMcuAdcDev *)handle;

    if (__get_IPSR() != 0U || LL_ADC_IsEnabled(CUR_ADC))
        return false;
    if (!vt_open(dev->vt_adc_buf) || !current_open(dev->cur_adc_buf))
        return false;

    return true;
}

static void sample_point(void *handle, uint32_t tick) {
    (void)handle;
    CUR_SET_SP(tick);
}

static bool sample_current(void *handle, float raw[3]) {
    if (!handle)
        return false;
    tMcuAdcDev *dev = (tMcuAdcDev *)handle;
    raw[0] = (dev->cur_adc_buf[CURRENT_U_CH] & 0xfffU) * current_scale;
    raw[1] = (dev->cur_adc_buf[CURRENT_V_CH] & 0xfffU) * current_scale;
    raw[2] = (dev->cur_adc_buf[CURRENT_W_CH] & 0xfffU) * current_scale;
    return true;
}

static bool sample_vt(void *handle, float *vbus, uint8_t *temp) {
    if (!handle)
        return false;
    tMcuAdcDev *dev = (tMcuAdcDev *)handle;
    if (vbus)
        *vbus = dev->vt_adc_buf[VBUS_CH] * vbus_scale;
    if (temp)
        *temp = TEMP_TABLE[dev->vt_adc_buf[TEMP_CH] & 0xff];
    return true;
}

const tSampleOps sen_mcu_ops = {
    .open = sample_open,
    .set_sample_point = sample_point,
    .get_cur_raw = sample_current,
    .get_vt = sample_vt,
};

static const tSampleDevHandle s_dev_handle = {
    .ops = &sen_mcu_ops,
    .dev = &mcu_adc,
};

const tSampleDevHandle *Get_MCU_ADCHandle(void) { return &s_dev_handle; }
