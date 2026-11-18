#include "enc_sincos.h"

#include "bsp_enc_adc.h"
#include "bsp_mcu.h"
#include "xmath.h"

static bool sincos_init(void *dev, void *cfg);
static bool sincos_open(void *dev);
static bool sincos_close(void *dev);
static bool sincos_read(void *dev, tEncSample *sample);

typedef struct SINCOS_ENCODER_DEVICE {
    tSinCosEncCfg cfg;
    float angle;
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
    tSinCosEncDev *enc = (tSinCosEncDev *)dev;
    tSinCosEncCfg *_cfg = (tSinCosEncCfg *)cfg;
    memcpy(&enc->cfg, _cfg, sizeof(tSinCosEncCfg));

    return true;
}

static bool sincos_open(void *dev) {
    if (!dev)
        return false;

    if (!bsp_enc_adc_sincos_open())
        return false;
    (void)dev;
    return true;
}

static bool sincos_close(void *dev) {
    if (!dev)
        return false;
    (void)dev;
    return true;
}



static bool sincos_read(void *dev, tEncSample *sample) {
    tSinCosEncDev *enc = (tSinCosEncDev *)dev;

    uint16_t sin_raw, cos_raw;
    if (!bsp_enc_adc_sincos_read(&sin_raw, &cos_raw))
        return false;

    float y = ((float)sin_raw - enc->cfg.sin_offset) * enc->cfg.sin_gain;
    float x = ((float)cos_raw - enc->cfg.cos_offset) * enc->cfg.cos_gain;

    enc->angle = XATAN2(y, x);
    enc->angle = XANGLE_2PI(enc->angle);

    tEncSample next = {
        .ts_us = bsp_time_us(),
        .angle_kind = BSP_ANGLE_MECHANICAL,
        .angle_rad = enc->angle,
    };
    *sample = next;
    return true;
}
