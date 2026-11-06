#include "enc_sincos.h"

#include "bsp_enc_adc.h"
#include "bsp_mcu.h"
#include "xmath.h"

static bool sincos_init(void *handle);
static bool sincos_open(void *handle);
static bool sincos_close(void *handle);
static bool sincos_read(void *handle, tEncSample *sample);

typedef struct SINCOS_ENCODER_CONFIG {
    uint32_t frequency_hz;
    float sin_offset, cos_offset; // 原始12位码域
    float sin_gain, cos_gain;     // 校正后的幅值比例，必须为正
    float min_amplitude;          // 校正后最小向量幅值
    bool use_cordic;              // true 要求整个程序独占 CORDIC
} tSinCosEncCfg;

// TODO:后续添加校准和参数配置
static tSinCosEncCfg sincos_cfg = {
    .frequency_hz = 1000,
    .sin_offset = 1.2f,
    .cos_offset = 0.0f,
    .sin_gain = 1.0f,
    .cos_gain = 1.0f,
    .min_amplitude = 100.0f,
    .use_cordic = false,
};

typedef struct SINCOS_ENCODER_DEVICE {
    bool s_open;
} tSinCosEncDev;

static tSinCosEncDev sincos_enc;

static const tEncoderOps enc_sincos_ops = {
    .init = sincos_init,
    .open = sincos_open,
    .read = sincos_read,
    .close = sincos_close,
};

static const tEncDevHandle s_sincos_handle = {
    .dev = &sincos_enc,
    .ops = &enc_sincos_ops,
};

const tEncDevHandle *enc_sincos_handle(void) { return &s_sincos_handle; }

static bool sincos_init(void *handle) {
    (void)handle;
    return true;
}

static bool sincos_open(void *handle) {
    if (!handle)
        return false;
    // 参数检查
    if (!isfinite(sincos_cfg.sin_offset) || !isfinite(sincos_cfg.cos_offset) ||
        !isfinite(sincos_cfg.sin_gain) || !isfinite(sincos_cfg.cos_gain) ||
        !isfinite(sincos_cfg.min_amplitude) || sincos_cfg.sin_gain <= 0.0f ||
        sincos_cfg.cos_gain <= 0.0f || sincos_cfg.min_amplitude <= 0.0f)
        return false;

    if (!bsp_enc_adc_sincos_open())
        return false;

    tSinCosEncDev *inst = (tSinCosEncDev *)handle;
    inst->s_open = true;
    return true;
}

static bool sincos_close(void *handle) {
    if (!handle)
        return false;
    tSinCosEncDev *inst = (tSinCosEncDev *)handle;
    inst->s_open = false;
    return true;
}

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
    if (!handle || !sample)
        return false;
    tSinCosEncDev *inst = (tSinCosEncDev *)handle;
    if (!inst->s_open)
        return false;

    uint16_t sin_raw, cos_raw;
    if (!bsp_enc_adc_sincos_read(&sin_raw, &cos_raw))
        return false;

    float y = ((float)sin_raw - sincos_cfg.sin_offset) * sincos_cfg.sin_gain;
    float x = ((float)cos_raw - sincos_cfg.cos_offset) * sincos_cfg.cos_gain;
    float amplitude = hypotf(x, y);
    if (!isfinite(amplitude) || amplitude < sincos_cfg.min_amplitude)
        return false;

    float angle;
    if (sincos_cfg.use_cordic) {
        if (!cordic_phase(x, y, &angle))
            return false;
    } else {
        angle = atan2f(y, x);
    }
    if (angle < 0.0f)
        angle += 6.28318530718f;
    if (angle >= 6.28318530718f)
        angle = 0.0f;

    tEncSample next = {
        .ts_us = bsp_time_us(),
        .angle_rad = angle,
        .angle_kind = BSP_ANGLE_SENSOR_PHASE,
        .referenced = true, // 传感器周期相位，不宣称机械单圈绝对位置
        .sin_raw = sin_raw,
        .cos_raw = cos_raw,
    };
    *sample = next;
    return true;
}
