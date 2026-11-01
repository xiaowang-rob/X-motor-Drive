#include "bsp_enc_adc.h"

#include "adc.h"
#include "xmath.h"

static bool sincos_open(void *handle);
static bool sincos_close(void *handle);
static bool sincos_read(void *handle, tEncSample *sample);
static void sincos_abort(void *handle);

typedef struct SINCOS_ENCODER_CONFIG {
    uint32_t frequency_hz;
    float sin_offset, cos_offset; // 原始12位码域
    float sin_gain, cos_gain;     // 校正后的幅值比例，必须为正
    float min_amplitude;          // 校正后最小向量幅值
    bool use_cordic;              // true 要求整个程序独占 CORDIC
} tSinCosEncCfg;
// TODO:后续添加校准和参数配置
tSinCosEncCfg sincos_cfg = {
    .frequency_hz = 1000,
    .sin_offset = 1.2,
};

typedef struct SINCOS_ENCODER_DEVICE {
    bool s_open;
} tSinCosEncDev;
tSinCosEncDev sincos_enc;

const tEncoderOps bsp_sincos_enc_ops = {
    .open = sincos_open,
    .read = sincos_read,
    .close = sincos_close,
    .abort = sincos_abort,
};

const tEncDevHandle SINCOS_ENCHandle = {
    .dev = &sincos_enc,
    .ops = &bsp_sincos_enc_ops,
};
const tEncDevHandle *Get_SINCOS_ENCHandle(void) { return &SINCOS_ENCHandle; }

static bool sincos_open(void *handle) {
    if (!handle)
        return false;
    // 参数检查
    if (!isfinite(sincos_cfg.sin_offset) || !isfinite(sincos_cfg.cos_offset) ||
        !isfinite(sincos_cfg.sin_gain) || !isfinite(sincos_cfg.cos_gain) ||
        !isfinite(sincos_cfg.min_amplitude) || sincos_cfg.sin_gain <= 0.0f ||
        sincos_cfg.cos_gain <= 0.0f || sincos_cfg.min_amplitude <= 0.0f)
        return false;

    // 汇总层已停止 SPI/TIM3；ADC 模拟输入无需 AF 编号。
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_6, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_7, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_6, LL_GPIO_MODE_ANALOG);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_7, LL_GPIO_MODE_ANALOG);
    // TODO：后续添加adc配置
    return true;
}

static bool sincos_close(void *handle) { return true; }

static bool cordic_phase(float x, float y, float *angle) {
    // 独占 CORDIC：Phase=2，6轮(24次迭代)，两个Q1.31参数，一个结果。
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_CORDIC);
    CORDIC->CSR = 2U | (6U << CORDIC_CSR_PRECISION_Pos) | CORDIC_CSR_NARGS;
    float scale = fmaxf(fabsf(x), fabsf(y));
    CORDIC->WDATA = (uint32_t)(int32_t)((x / scale) * 1073741824.0f);
    CORDIC->WDATA = (uint32_t)(int32_t)((y / scale) * 1073741824.0f);
    uint32_t timeout = 10000U;
    while (!(CORDIC->CSR & CORDIC_CSR_RRDY))
        if (--timeout == 0U)
            return false;
    *angle = (int32_t)CORDIC->RDATA * (3.14159265359f / 2147483648.0f);
    return true;
}
static bool sincos_read(void *handle, tEncSample *sample) {
    if (!sample || !handle)
        return false;

    // tBspAuxFrame frame;
    // if (!bsp_aux_adc_read(&frame) || !frame.sincos)
    //     return BSP_ENC_BUSY;
    // tBspEncSample next = {
    //     .sin_raw = frame.sin, .cos_raw = frame.cos, .angle_kind =
    //     BSP_ANGLE_SENSOR_PHASE, .ts_ms = HAL_GetTick()};
    // float y = (frame.sin - s_cfg.sin_offset) * s_cfg.sin_gain;
    // float x = (frame.cos - s_cfg.cos_offset) * s_cfg.cos_gain;
    // float amplitude = hypotf(x, y);
    // if (!isfinite(amplitude) || amplitude < s_cfg.min_amplitude)
    //     return BSP_ENC_IO_ERROR;
    // if (s_cfg.use_cordic) {
    //     if (!cordic_phase(x, y, &next.angle_rad))
    //         return BSP_ENC_IO_ERROR;
    // } else
    //     next.angle_rad = atan2f(y, x);
    // if (next.angle_rad < 0.0f)
    //     next.angle_rad += 6.28318530718f;
    // if (next.angle_rad >= 6.28318530718f)
    //     next.angle_rad = 0.0f;
    // next.referenced = true; // 传感器周期相位，不宣称机械单圈绝对位置
    // *sample = next;
    return true;
}
static void sincos_abort(void *handle) { /* 同步读取没有在途事务 */ }
