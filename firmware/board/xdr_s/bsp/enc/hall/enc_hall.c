#include "enc_hall.h"

#include "bsp_enc_tim.h"
#include "bsp_mcu.h"
#include "main.h"
#include "xmath.h"

static bool hall_enc_init(void *dev, void *cfg);
static bool hall_enc_open(void *dev);
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
    uint8_t
        sequence[8]; // 电角度正向 Hall 状态序列，必须为1..6的合法单比特相邻环
    uint16_t prescaler; // 捕获时基 PSC，不是 PWM 分频
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
    for (unsigned i = 0; i < 8; ++i)
        if (dev->sequence[i] == state)
            return (int)i;
    return -1;
}

static bool hall_enc_init(void *dev, void *cfg) {
    if (!dev || !cfg)
        return false;
    tHallEncDev *inst = (tHallEncDev *)dev;
    tHallEncCfg *_cfg = (tHallEncCfg *)cfg;
    memcpy(inst->sequence, _cfg->sequence, 8);
    inst->prescaler = _cfg->prescaler;
    inst->filter = (uint8_t)(_cfg->filter * 225 + 0.5f);
    return true;
}

// TODO:换成8个成员
static bool hall_enc_open(void *dev) {
    if (!dev)
        return false;
    tHallEncDev *inst = (tHallEncDev *)dev;

    uint8_t seen = 0;
    for (unsigned i = 0; i < 6; ++i) {
        unsigned state = inst->sequence[i];
        unsigned delta = state ^ inst->sequence[(i + 1U) % 6U];
        if (state == 0U || state > 6U || (seen & (1U << state)) ||
            delta == 0U || (delta & (delta - 1U)))
            return false;
        seen |= (uint8_t)(1U << state);
    }

    bsp_tim_enc_init();
    int initial = sector(inst, hall_state());
    if (initial < 0)
        return false;
    const tTimEncConfig cfg = {
        .psc = inst->prescaler,
        .arr = 0xffffU,
        // TI1S: CH1/2/3 XOR；TI1F_ED 对任意 Hall 边沿产生触发。
        .cr2 = TIM_CR2_TI1S,
        // CC1S=TRC：捕获 TI1F_ED 的触发脉冲，与硬件复位共用事件。
        // IC1F 配置 TI1 输入滤波；实际毛刺抑制效果需要在板上验证。
        .ccmr1 =
            TIM_CCMR1_CC1S | ((uint32_t)inst->filter << TIM_CCMR1_IC1F_Pos),
        // TS=TI1F_ED(100)，SMS=Reset(100)：捕获旧计数并硬件清零。
        .smcr = (4U << TIM_SMCR_TS_Pos) | (4U << TIM_SMCR_SMS_Pos),
        // TI1F_ED 已将 XOR 的双边沿转换为脉冲；TRC 只捕获脉冲上升沿。
        .ccer = TIM_CCER_CC1E,
        .dier = TIM_DIER_CC1IE | TIM_DIER_UIE,
        // URS=1：从模式复位不产生更新中断，仅溢出才报告超时。
        .cr1 = TIM_CR1_URS | TIM_CR1_CEN,
    };
    inst->s_sector = (uint8_t)initial;
    inst->s_direction = 0;
    inst->s_interval = 0;
    inst->s_timestamp = HAL_GetTick();
    inst->s_error = inst->s_overflow = false;
    inst->s_open = true;
    bsp_tim_register_cb(hall_enc_irq, inst);
    bsp_tim_enc_configure(&cfg);
    return true;
}

static void hall_enc_irq(void *dev) {
    if (!dev)
        return;
    tHallEncDev *inst = (tHallEncDev *)dev;

    uint32_t status = bsp_tim_enc_status();
    bsp_tim_enc_clear_status(status &
                             (TIM_SR_CC1IF | TIM_SR_CC1OF | TIM_SR_UIF));
    if (!inst->s_open)
        return;
    if (status & TIM_SR_UIF)
        inst->s_overflow = true;
    if (status & TIM_SR_CC1OF)
        inst->s_error = true; // ISR 来不及，禁止继续输出可信角度
    if (status & TIM_SR_CC1IF) {
        uint32_t captured = bsp_tim_enc_capture(1U);
        int next = sector(inst, hall_state());
        if (next < 0) {
            inst->s_error = true;
            return;
        }
        unsigned distance = ((unsigned)next + 6U - inst->s_sector) % 6U;
        if (distance != 1U && distance != 5U) {
            inst->s_error = true;
            return;
        }
        inst->s_direction = distance == 1U ? 1 : -1;
        inst->s_sector = (uint8_t)next;
        inst->s_interval = inst->s_overflow ? 0U : captured;
        inst->s_overflow = false;
        inst->s_timestamp = bsp_time_us();
    }
}

static bool hall_enc_read(void *dev, tEncSample *sample) {
    if (!dev || !sample)
        return false;
    tHallEncDev *inst = (tHallEncDev *)dev;

    if (!inst->s_open)
        return false;
    uint32_t saved = __get_PRIMASK();
    __disable_irq();
    int now = sector(inst, hall_state());
    bool error = inst->s_error || now < 0;
    if (error)
        return false;
    sample->raw = inst->sequence[inst->s_sector];
    sample->angle_rad = (inst->s_sector + 0.5f) * (XMATH_2PI / 6.0f);
    sample->angle_kind = BSP_ANGLE_ELECTRICAL;
    sample->interval_ticks = inst->s_overflow ? 0U : inst->s_interval;
    sample->direction = inst->s_direction;
    sample->ts_us = inst->s_timestamp;

    __set_PRIMASK(saved);

    return true;
}

static bool hall_enc_close(void *dev) {
    if (!dev)
        return false;
    tHallEncDev *inst = (tHallEncDev *)dev;

    inst->s_open = false;
    bsp_tim_enc_stop();
    bsp_tim_register_cb(NULL, NULL);
    return true;
}
