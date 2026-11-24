#include "enc_hall.h"

#include "bsp_enc_tim.h"
#include "bsp_mcu.h"
#include "main.h"
#include "bsp_math.h"

static bool hall_enc_init(void *dev);
static bool hall_enc_open(void *dev, void *cfg);
static bool hall_enc_close(void *dev);
static bool hall_enc_read(void *dev, tEncSample *sample);
static void hall_enc_irq(void *dev);

static const tEncoderOps enc_hall_ops = {
    .init = hall_enc_init,
    .open = hall_enc_open,
    .close = hall_enc_close,
    .read = hall_enc_read,
};

typedef struct HALL_ENCODER_DEVICE {
    tHallEncCfg cfg;
    uint8_t filter;

    volatile bool s_open, s_error, s_overflow;
    volatile uint8_t s_sector;
    volatile int8_t s_direction;
    volatile uint32_t s_interval, s_timestamp;
} tHallEncDev;

static tHallEncDev hall_enc;

static const tEncDevHandle s_hall_handle = {
    .ops = &enc_hall_ops,
    .dev = &hall_enc,
};

const tEncDevHandle *Get_EncHallHandle(void) { return &s_hall_handle; }

static uint8_t hall_state(void) {
    return ((GPIOA->IDR >> 6U) & 3U) | ((GPIOB->IDR & 1U) << 2U);
}

static int sector(tHallEncDev *dev, uint8_t state) {
    for (unsigned i = 0; i < 6; ++i)
        if (dev->cfg.sequence[i] == state)
            return (int)i;
    return -1;
}

static bool hall_enc_init(void *dev) {
    if (!dev)
        return false;
    //  tHallEncDev *enc = (tHallEncDev *)dev;

    // 硬件链路初始化
    bsp_tim_enc_init();
    return true;
}

static bool hall_enc_open(void *dev, void *cfg) {
    if (!dev || !cfg)
        return false;
    tHallEncDev *enc = (tHallEncDev *)dev;
    tHallEncCfg *_cfg = (tHallEncCfg *)cfg;
    // 加载编码器配置
    memcpy(&enc->cfg, _cfg, sizeof(tHallEncCfg));
    enc->filter = (uint8_t)(enc->cfg.filter * 225 + 0.5f);

    // 校验配置
    uint8_t seen = 0;
    for (unsigned i = 0; i < 6; ++i) {
        unsigned state = enc->cfg.sequence[i];
        unsigned delta = state ^ enc->cfg.sequence[(i + 1U) % 6];
        if (state >= 6 || (seen & (1U << state)) || delta == 0U ||
            (delta & (delta - 1U)))
            return false;
        seen |= (uint8_t)(1U << state);
    }
    // 获取初始相位
    int initial = sector(enc, hall_state());
    if (initial < 0)
        return false;
    // 配置时钟
    const tTimEncConfig tim_cfg = {
        .psc = enc->cfg.prescaler,
        .arr = 0xffffU,
        // TI1S: CH1/2/3 XOR；TI1F_ED 对任意 Hall 边沿产生触发。
        .cr2 = TIM_CR2_TI1S,
        // CC1S=TRC：捕获 TI1F_ED 的触发脉冲，与硬件复位共用事件。
        // IC1F 配置 TI1 输入滤波；实际毛刺抑制效果需要在板上验证。
        .ccmr1 = TIM_CCMR1_CC1S | ((uint32_t)enc->filter << TIM_CCMR1_IC1F_Pos),
        // TS=TI1F_ED(100)，SMS=Reset(100)：捕获旧计数并硬件清零。
        .smcr = (4U << TIM_SMCR_TS_Pos) | (4U << TIM_SMCR_SMS_Pos),
        // TI1F_ED 已将 XOR 的双边沿转换为脉冲；TRC 只捕获脉冲上升沿。
        .ccer = TIM_CCER_CC1E,
        .dier = TIM_DIER_CC1IE | TIM_DIER_UIE,
        // URS=1：从模式复位不产生更新中断，仅溢出才报告超时。
        .cr1 = TIM_CR1_URS | TIM_CR1_CEN,
    };
    enc->s_sector = (uint8_t)initial;
    enc->s_direction = 0;
    enc->s_interval = 0;
    enc->s_timestamp = HAL_GetTick();
    enc->s_error = enc->s_overflow = false;
    enc->s_open = true;
    bsp_tim_register_cb(hall_enc_irq, enc);
    bsp_tim_enc_configure(&tim_cfg);

    return true;
}

static void hall_enc_irq(void *dev) {
    if (!dev)
        return;
    tHallEncDev *enc = (tHallEncDev *)dev;

    uint32_t status = bsp_tim_enc_status();
    bsp_tim_enc_clear_status(status &
                             (TIM_SR_CC1IF | TIM_SR_CC1OF | TIM_SR_UIF));
    if (!enc->s_open)
        return;
    if (status & TIM_SR_UIF)
        enc->s_overflow = true;
    if (status & TIM_SR_CC1OF)
        enc->s_error = true; // ISR 来不及，禁止继续输出可信角度
    if (status & TIM_SR_CC1IF) {
        uint32_t captured = bsp_tim_enc_capture(1U);
        int next = sector(enc, hall_state());
        if (next < 0) {
            enc->s_error = true;
            return;
        }
        unsigned distance = ((unsigned)next + 6 - enc->s_sector) % 6;
        if (distance != 1U && distance != 6 - 1U) {
            enc->s_error = true;
            return;
        }
        enc->s_direction = distance == 1U ? 1 : -1;
        enc->s_sector = (uint8_t)next;
        enc->s_interval = enc->s_overflow ? 0U : captured;
        enc->s_overflow = false;
        enc->s_timestamp = bsp_time_us();
    }
}

static bool hall_enc_read(void *dev, tEncSample *sample) {

    tHallEncDev *enc = (tHallEncDev *)dev;

    if (!enc->s_open)
        return false;
    uint32_t saved = __get_PRIMASK();
    __disable_irq();
    int now = sector(enc, hall_state());
    bool error = enc->s_error || now < 0;
    if (error) {
        __set_PRIMASK(saved);
        return false;
    }
    sample->raw = enc->cfg.sequence[enc->s_sector];
    sample->angle_rad = (enc->s_sector + 0.5f) * (MATH_2PI / 6);
    sample->angle_kind = BSP_ANGLE_ELECTRICAL;
    sample->interval_ticks = enc->s_overflow ? 0U : enc->s_interval;
    sample->direction = enc->s_direction;
    sample->ts_us = enc->s_timestamp;

    __set_PRIMASK(saved);

    return true;
}

static bool hall_enc_close(void *dev) {
    if (!dev)
        return false;
    tHallEncDev *enc = (tHallEncDev *)dev;

    enc->s_open = false;
    bsp_tim_enc_stop();
    bsp_tim_register_cb(NULL, NULL);
    return true;
}
