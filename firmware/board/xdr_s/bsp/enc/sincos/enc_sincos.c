#include "enc_sincos.h"

#include "bsp_enc_adc.h"
#include "bsp_mcu.h"
#include "xmath.h"

static bool sincos_init(void *dev, void *cfg);
static bool sincos_open(void *dev);
static bool sincos_close(void *dev);
static bool sincos_read(void *dev, tEncSample *sample);

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

const tEncDevHandle *Get_EncSinCosHandle(void) { return &s_sincos_handle; }

static bool sincos_init(void *dev, void *cfg) {
    (void)dev;
    (void)cfg;
    return true;
}

static bool sincos_open(void *dev) {
    if (!dev)
        return false;

    if (!bsp_enc_adc_sincos_open())
        return false;

    tSinCosEncDev *inst = (tSinCosEncDev *)dev;
    inst->s_open = true;
    return true;
}

static bool sincos_close(void *dev) {
    if (!dev)
        return false;
    tSinCosEncDev *inst = (tSinCosEncDev *)dev;
    inst->s_open = false;
    return true;
}

// static bool cordic_phase(float x, float y, float *angle) {
//     // 独占 CORDIC：Phase=2，6轮(24次迭代)，两个Q1.31参数，一个结果。
//     LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_CORDIC);
//     CORDIC->CSR = 2U | (6U << CORDIC_CSR_PRECISION_Pos) | CORDIC_CSR_NARGS;
//     float scale = fmaxf(fabsf(x), fabsf(y));
//     CORDIC->WDATA = (uint32_t)(int32_t)((x / scale) * 1073741824.0f);
//     CORDIC->WDATA = (uint32_t)(int32_t)((y / scale) * 1073741824.0f);
//     uint32_t timeout = 10000U;
//     while (!(CORDIC->CSR & CORDIC_CSR_RRDY))
//         if (--timeout == 0U)
//             return false;
//     *angle = (int32_t)CORDIC->RDATA * (3.14159265359f / 2147483648.0f);
//     return true;
// }

static bool sincos_read(void *dev, tEncSample *sample) {
    if (!dev || !sample)
        return false;
    tSinCosEncDev *inst = (tSinCosEncDev *)dev;
    if (!inst->s_open)
        return false;

    uint16_t sin_raw, cos_raw;
    if (!bsp_enc_adc_sincos_read(&sin_raw, &cos_raw))
        return false;

    tEncSample next = {
        .ts_us = bsp_time_us(),
        .angle_kind = BSP_ANGLE_MECHANICAL,
        .sin_raw = sin_raw,
        .cos_raw = cos_raw,
    };
    *sample = next;

    // float y = ((float)sin_raw - sincos_cfg.sin_offset) * sincos_cfg.sin_gain;
    // float x = ((float)cos_raw - sincos_cfg.cos_offset) * sincos_cfg.cos_gain;
    // float amplitude = hypotf(x, y);
    // if (!isfinite(amplitude) || amplitude < sincos_cfg.min_amplitude)
    //     return false;

    // float angle;
    // if (sincos_cfg.use_cordic) {
    //     if (!cordic_phase(x, y, &angle))
    //         return false;
    // } else {
    //     angle = atan2f(y, x);
    // }
    // if (angle < 0.0f)
    //     angle += 6.28318530718f;
    // if (angle >= 6.28318530718f)
    //     angle = 0.0f;

    return true;
}
