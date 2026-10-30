#include "bsp_enc_tim.h"
#include "tim.h"

static bool abi_enc_open(void *handle);
static bool abi_enc_read(void *handle, tEncSample *sample);
static bool abi_enc_close(void *handle);
static void abi_enc_abort(void *handle);

const tEncoderOps abi_enc_ops = {
    .open = abi_enc_open,
    .read = abi_enc_read,
    .close = abi_enc_close,
    .abort = abi_enc_abort,
};

// TODO:后续改为由参数配置写入
#define ABI_COUNTS 65535
#define ABI_FILTER_ALPHA 0.1f
#define ABI_INDEX_FALL false

typedef struct ABI_CONFIG {
    uint32_t counts_per_rev; // A/B 四倍频后的每机械圈计数，1..65536
    uint8_t filter;          // TIM ICxF，0..15
    bool index_falling;
} tAbiCfg;

tAbiCfg abi_cfg = {
    .counts_per_rev = ABI_COUNTS,
    .filter = (uint8_t)(ABI_FILTER_ALPHA * 15 + 0.5f),
    .index_falling = ABI_INDEX_FALL,
};

typedef struct ABI_ENCODER_DEVICE {
    volatile bool s_open, s_index, s_error; // index(I) 状态
} tAbiEncDev;

tAbiEncDev abi_enc;

const static tEncDevHandle abi_enc_handle = {
    .ops = &abi_enc_ops,
    .dev = &abi_enc,
};
tEncDevHandle *Get_ABI_ENCHandle(void) { return &abi_enc_handle; };

static bool abi_enc_open(void *handle) {
    if (abi_cfg.counts_per_rev > 65535U)
        return false;
    if (!handle)
        return false;
    tAbiEncDev *inst = (tAbiEncDev *)handle;
    TIM3->CR1 = 0U;
    TIM3->CR2 = 0U; // 禁用 Hall XOR
    TIM3->PSC = 0U;
    TIM3->ARR = abi_cfg.counts_per_rev;
    // CC1S/CC2S=直接输入，SMS=011：TI1&TI2，A/B 双边沿四倍频。
    TIM3->CCMR1 = TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0 |
                  ((uint32_t)abi_cfg.filter << TIM_CCMR1_IC1F_Pos) |
                  ((uint32_t)abi_cfg.filter << TIM_CCMR1_IC2F_Pos);
    TIM3->SMCR = 3U << TIM_SMCR_SMS_Pos;
    TIM3->CCMR2 =
        TIM_CCMR2_CC3S_0 | ((uint32_t)abi_cfg.filter << TIM_CCMR2_IC3F_Pos);
    TIM3->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E |
                 (abi_cfg.index_falling ? TIM_CCER_CC3P : 0U);
    TIM3->EGR = TIM_EGR_UG;
    TIM3->CNT = 0U;
    TIM3->SR = 0U;
    inst->s_index = inst->s_error = false;
    inst->s_open = true;
    TIM3->DIER = TIM_DIER_CC3IE;
    TIM3->CR1 = TIM_CR1_CEN;
    return true;
}

void abi_enc_irq(void *handle) {
    if (!handle)
        return;
    tAbiEncDev *inst = (tAbiEncDev *)handle;

    uint32_t status = TIM3->SR;
    // RC_W0：仅清除本次看见的标志，避免误清新到达的事件。
    TIM3->SR = ~(status & (TIM_SR_CC3IF | TIM_SR_CC3OF));
    if (!inst->s_open)
        return;
    if (status & TIM_SR_CC3OF)
        inst->s_error = true;
    if (status & TIM_SR_CC3IF) {
        (void)TIM3->CCR3;
        // 软件 Index 清零存在 ISR 延迟；不是边沿同步的硬件清零。
        TIM3->CNT = 0U;
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
    tEncSample next = {.raw = (uint16_t)TIM3->CNT,
                       .resolution = abi_cfg.counts_per_rev,
                       .referenced = inst->s_index,
                       .angle_kind = inst->s_index ? BSP_ANGLE_MECHANICAL
                                                   : BSP_ANGLE_RELATIVE,
                       .ts_ms = HAL_GetTick()};
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
    TIM3->DIER = 0U;
    TIM3->CCER = 0U;
    TIM3->CR1 = 0U;
    return true;
}

static void abi_enc_abort(void *handle) {
    /* 无在途事务，重新 open 重置参考状态 */
}
