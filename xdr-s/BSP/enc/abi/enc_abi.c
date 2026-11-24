#include "enc_abi.h"

#include "bsp_enc_tim.h"
#include "bsp_mcu.h"
#include "main.h"
#include "bsp_math.h"

static bool abi_enc_init(void *dev);
static bool abi_enc_open(void *dev, void *cfg);
static bool abi_enc_read(void *dev, tEncSample *sample);
static bool abi_enc_close(void *dev);
static void abi_enc_irq(void *dev);

static const tEncoderOps enc_abi_ops = {
    .init = abi_enc_init,
    .open = abi_enc_open,
    .read = abi_enc_read,
    .close = abi_enc_close,
};

#define ABI_MAX_COUNTS 65536U // 16位 计时器的最大限制
#define ABI_TIM_DECODING 4U   // 四倍频 AB上升下降都采集

typedef struct ABI_ENCODER_DEVICE {
    tAbiCfg cfg;
    uint32_t counts_per_rev;        // A/B 4x每机械圈计数，1..65536
    uint8_t filter;                 // TIM ICxF，0..15
    volatile bool s_index, s_error; // index(I) 状态
} tAbiEncDev;

static tAbiEncDev abi_enc;

static const tEncDevHandle s_abi_handle = {
    .ops = &enc_abi_ops,
    .dev = &abi_enc,
};

const tEncDevHandle *Get_EncAbiHandle(void) { return &s_abi_handle; }

static bool abi_enc_init(void *dev) {
    if (!dev)
        return false;
    (void)dev;
    // 初始化硬件链路
    bsp_tim_enc_init();
    return true;
}

static bool abi_enc_open(void *dev, void *cfg) {
    if (!dev || !cfg)
        return false;
    tAbiEncDev *enc = (tAbiEncDev *)dev;
    tAbiCfg *_cfg = (tAbiCfg *)cfg;
    // 加载编码器配置
    memcpy(&enc->cfg, _cfg, sizeof(tAbiCfg));

    enc->counts_per_rev = MATH_CLAMP(
        enc->cfg.enc_counts_per_rev * ABI_TIM_DECODING, 1, ABI_MAX_COUNTS);
    enc->filter = (uint8_t)(enc->cfg.filter * 15 + 0.5f);
    // 时钟配置
    const tTimEncConfig tim_cfg = {
        .psc = 0U,
        .arr = enc->counts_per_rev - 1U,
        .cr2 = 0U, // 禁用 Hall XOR
        // CC1S/CC2S=直接输入，SMS=011：TI1&TI2，A/B 双边沿四倍频。
        .ccmr1 = TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0 |
                 ((uint32_t)enc->filter << TIM_CCMR1_IC1F_Pos) |
                 ((uint32_t)enc->filter << TIM_CCMR1_IC2F_Pos),
        .ccmr2 =
            TIM_CCMR2_CC3S_0 | ((uint32_t)enc->filter << TIM_CCMR2_IC3F_Pos),
        .smcr = 3U << TIM_SMCR_SMS_Pos,
        .ccer = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E |
                (enc->cfg.index_falling ? TIM_CCER_CC3P : 0U),
        .dier = TIM_DIER_CC3IE,
        .cr1 = TIM_CR1_CEN,
    };
    // 注册中断 并 加载时钟配置
    bsp_tim_register_cb(abi_enc_irq, enc);
    bsp_tim_enc_configure(&tim_cfg);

    enc->s_index = enc->s_error = false;
    return true;
}

static void abi_enc_irq(void *dev) {
    if (!dev)
        return;
    tAbiEncDev *enc = (tAbiEncDev *)dev;

    uint32_t status = bsp_tim_enc_status();
    bsp_tim_enc_clear_status(status & (TIM_SR_CC3IF | TIM_SR_CC3OF));
    if (status & TIM_SR_CC3OF)
        enc->s_error = true;
    if (status & TIM_SR_CC3IF) {
        (void)bsp_tim_enc_capture(3U);
        // 软件 Index 清零存在 ISR 延迟；不是边沿同步的硬件清零。
        bsp_tim_enc_set_count(0U);
        enc->s_index = true;
    }
}

static bool abi_enc_read(void *dev, tEncSample *sample) {

    tAbiEncDev *enc = (tAbiEncDev *)dev;

    if (enc->s_error)
        return false;

    uint32_t saved = __get_PRIMASK();
    __disable_irq();
    uint16_t raw = (uint16_t)bsp_tim_enc_count();
    sample->angle_rad = raw * (MATH_2PI / enc->counts_per_rev);
    sample->referenced = enc->s_index;
    sample->angle_kind = BSP_ANGLE_MECHANICAL;
    sample->ts_us = bsp_time_us();
    __set_PRIMASK(saved);

    return true;
}

static bool abi_enc_close(void *dev) {
    (void)dev;

    bsp_tim_enc_stop();
    bsp_tim_register_cb(NULL, NULL);
    return true;
}
