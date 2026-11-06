#include "enc_abi.h"

#include "bsp_enc_tim.h"
#include "bsp_mcu.h"
#include "tim.h"

static bool abi_enc_init(void *handle);
static bool abi_enc_open(void *handle);
static bool abi_enc_read(void *handle, tEncSample *sample);
static bool abi_enc_close(void *handle);
static void abi_enc_irq(void *handle);

static const tEncoderOps enc_abi_ops = {
    .init = abi_enc_init,
    .open = abi_enc_open,
    .read = abi_enc_read,
    .close = abi_enc_close,
};

// TODO:后续改为由参数配置写入
#define ABI_COUNTS 65536U
#define ABI_FILTER_ALPHA 0.1f
#define ABI_INDEX_FALL false

typedef struct ABI_CONFIG {
    uint32_t counts_per_rev; // A/B 四倍频后的每机械圈计数，1..65536
    uint8_t filter;          // TIM ICxF，0..15
    bool index_falling;
} tAbiCfg;

static tAbiCfg abi_cfg = {
    .counts_per_rev = ABI_COUNTS,
    .filter = (uint8_t)(ABI_FILTER_ALPHA * 15 + 0.5f),
    .index_falling = ABI_INDEX_FALL,
};

typedef struct ABI_ENCODER_DEVICE {
    volatile bool s_open, s_index, s_error; // index(I) 状态
} tAbiEncDev;

static tAbiEncDev abi_enc;

static const tEncDevHandle s_abi_handle = {
    .ops = &enc_abi_ops,
    .dev = &abi_enc,
};

const tEncDevHandle *enc_abi_handle(void) { return &s_abi_handle; }

static bool abi_enc_init(void *handle) {
    (void)handle;
    return true;
}

static bool abi_enc_open(void *handle) {
    if (abi_cfg.counts_per_rev == 0U || abi_cfg.counts_per_rev > 65536U)
        return false;
    if (!handle)
        return false;
    tAbiEncDev *inst = (tAbiEncDev *)handle;

    bsp_tim_enc_init();
    const tTimEncConfig cfg = {
        .psc = 0U,
        .arr = abi_cfg.counts_per_rev - 1U,
        .cr2 = 0U, // 禁用 Hall XOR
        // CC1S/CC2S=直接输入，SMS=011：TI1&TI2，A/B 双边沿四倍频。
        .ccmr1 = TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0 |
                 ((uint32_t)abi_cfg.filter << TIM_CCMR1_IC1F_Pos) |
                 ((uint32_t)abi_cfg.filter << TIM_CCMR1_IC2F_Pos),
        .ccmr2 =
            TIM_CCMR2_CC3S_0 | ((uint32_t)abi_cfg.filter << TIM_CCMR2_IC3F_Pos),
        .smcr = 3U << TIM_SMCR_SMS_Pos,
        .ccer = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E |
                (abi_cfg.index_falling ? TIM_CCER_CC3P : 0U),
        .dier = TIM_DIER_CC3IE,
        .cr1 = TIM_CR1_CEN,
    };
    inst->s_index = inst->s_error = false;
    inst->s_open = true;
    bsp_tim_register_cb(abi_enc_irq, inst);
    bsp_tim_enc_configure(&cfg);
    return true;
}

static void abi_enc_irq(void *handle) {
    if (!handle)
        return;
    tAbiEncDev *inst = (tAbiEncDev *)handle;

    uint32_t status = bsp_tim_enc_status();
    bsp_tim_enc_clear_status(status & (TIM_SR_CC3IF | TIM_SR_CC3OF));
    if (!inst->s_open)
        return;
    if (status & TIM_SR_CC3OF)
        inst->s_error = true;
    if (status & TIM_SR_CC3IF) {
        (void)bsp_tim_enc_capture(3U);
        // 软件 Index 清零存在 ISR 延迟；不是边沿同步的硬件清零。
        bsp_tim_enc_set_count(0U);
        inst->s_index = true;
    }
}

static bool abi_enc_read(void *handle, tEncSample *sample) {
    if (!sample || !handle)
        return false;

    tAbiEncDev *inst = (tAbiEncDev *)handle;

    if (!inst->s_open)
        return false;

    uint32_t saved = __get_PRIMASK();
    __disable_irq();
    bool error = inst->s_error;
    tEncSample next = {.raw = (uint16_t)bsp_tim_enc_count(),
                       .resolution = abi_cfg.counts_per_rev,
                       .referenced = inst->s_index,
                       .angle_kind = inst->s_index ? BSP_ANGLE_MECHANICAL
                                                   : BSP_ANGLE_RELATIVE,
                       .ts_us = bsp_time_us()};
    __set_PRIMASK(saved);
    if (error)
        return false;
    next.angle_rad = next.raw * (6.28318530718f / next.resolution);
    *sample = next;
    return true;
}

static bool abi_enc_close(void *handle) {
    if (!handle)
        return false;
    tAbiEncDev *inst = (tAbiEncDev *)handle;

    inst->s_open = false;
    bsp_tim_enc_stop();
    bsp_tim_register_cb(NULL, NULL);
    return true;
}
