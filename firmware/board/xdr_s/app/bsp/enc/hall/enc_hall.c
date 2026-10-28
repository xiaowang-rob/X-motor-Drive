#include "bsp_enc_tim.h"
#include "tim.h"

static bool hall_enc_open(void *handle);
static bool hall_enc_close(void *handle);
static bool hall_enc_read(void *handle, tEncSample *sample);
static void hall_enc_abort(void *handle);

const tEncoderOps bsp_hall_enc_ops = {
    .open = hall_enc_open,
    .close = hall_enc_close,
    .read = hall_enc_read,
    .abort = hall_enc_abort,
};

// TODO:后续由参数配置
typedef struct HALL_ENCODER_CONFIG {
    uint8_t
        sequence[6]; // 电角度正向 Hall 状态序列，必须为1..6的合法单比特相邻环
    uint16_t prescaler; // 捕获时基 PSC，不是 PWM 分频
    uint8_t filter;
} tHallEncCfg;

tHallEncCfg hall_cfg = {
    .prescaler = 0,
    .filter = 0,
};
typedef struct HALL_ENCODER_DEVICE {
    volatile bool s_open, s_error, s_overflow;
    volatile uint8_t s_sector;
    volatile int8_t s_direction;
    volatile uint32_t s_interval, s_timestamp;
} tHallEncDev;

static tHallEncDev hall_enc;

const tEncDevHandle HALL_ENCHandle = {
    .ops = &bsp_hall_enc_ops,
    .dev = &hall_enc,
};
tEncDevHandle *Get_HALL_ENCHandle(void) { return &HALL_ENCHandle; }

static uint8_t hall_state(void) {
    return ((GPIOA->IDR >> 6U) & 3U) | ((GPIOB->IDR & 1U) << 2U);
}
static int sector(uint8_t state) {
    for (unsigned i = 0; i < 6; ++i)
        if (hall_cfg.sequence[i] == state)
            return (int)i;
    return -1;
}
static bool hall_enc_open(void *handle) {
    if (!handle)
        return false;
    tHallEncDev *inst = (tHallEncDev *)handle;

    uint8_t seen = 0;
    for (unsigned i = 0; i < 6; ++i) {
        unsigned state = hall_cfg.sequence[i];
        unsigned delta = state ^ hall_cfg.sequence[(i + 1U) % 6U];
        if (state == 0U || state > 6U || (seen & (1U << state)) ||
            delta == 0U || (delta & (delta - 1U)))
            return false;
        seen |= (uint8_t)(1U << state);
    }

    int initial = sector(hall_state());
    if (initial < 0)
        return false;
    TIM3->CR1 = 0U;
    TIM3->PSC = hall_cfg.prescaler;
    TIM3->ARR = 0xffffU;
    // TI1S: CH1/2/3 XOR；TI1F_ED 对任意 Hall 边沿产生触发。
    TIM3->CR2 = TIM_CR2_TI1S;
    // CC1S=TRC：捕获 TI1F_ED 的触发脉冲，与硬件复位共用事件。
    // IC1F 配置 TI1 输入滤波；实际毛刺抑制效果需要在板上验证。
    TIM3->CCMR1 =
        TIM_CCMR1_CC1S | ((uint32_t)hall_cfg.filter << TIM_CCMR1_IC1F_Pos);
    // TS=TI1F_ED(100)，SMS=Reset(100)：捕获旧计数并硬件清零。
    TIM3->SMCR = (4U << TIM_SMCR_TS_Pos) | (4U << TIM_SMCR_SMS_Pos);
    // TI1F_ED 已将 XOR 的双边沿转换为脉冲；TRC 只捕获脉冲上升沿。
    TIM3->CCER = TIM_CCER_CC1E;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->SR = 0U;
    inst->s_sector = (uint8_t)initial;
    inst->s_direction = 0;
    inst->s_interval = 0;
    inst->s_timestamp = HAL_GetTick();
    inst->s_error = inst->s_overflow = false;
    inst->s_open = true;
    TIM3->DIER = TIM_DIER_CC1IE | TIM_DIER_UIE;
    // URS=1：从模式复位不产生更新中断，仅溢出才报告超时。
    TIM3->CR1 = TIM_CR1_URS | TIM_CR1_CEN;
    return true;
}
void hall_enc_irq(void *handle) {
    if (!handle)
        return;
    tHallEncDev *inst = (tHallEncDev *)handle;

    uint32_t status = TIM3->SR;
    TIM3->SR = ~(status & (TIM_SR_CC1IF | TIM_SR_CC1OF | TIM_SR_UIF));
    if (!inst->s_open)
        return;
    if (status & TIM_SR_UIF)
        inst->s_overflow = true;
    if (status & TIM_SR_CC1OF)
        inst->s_error = true; // ISR 来不及，禁止继续输出可信角度
    if (status & TIM_SR_CC1IF) {
        uint32_t captured = TIM3->CCR1;
        int next = sector(hall_state());
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
        inst->s_timestamp = HAL_GetTick();
    }
}
static bool hall_enc_read(void *handle, tEncSample *sample) {
    if (!handle)
        return false;
    tHallEncDev *inst = (tHallEncDev *)handle;

    if (!sample)
        return false;
    if (!inst->s_open)
        return false;
    uint32_t saved = __get_PRIMASK();
    __disable_irq();
    int now = sector(hall_state());
    bool error = inst->s_error || now < 0;
    tEncSample next = {
        .raw = hall_cfg.sequence[inst->s_sector],
        .resolution = 6U,
        .angle_rad = (inst->s_sector + 0.5f) * (6.28318530718f / 6.0f),
        .angle_kind = BSP_ANGLE_ELECTRICAL,
        .referenced = true,
        .interval_ticks = inst->s_overflow ? 0U : inst->s_interval,
        .direction = inst->s_direction,
        .ts_ms = inst->s_timestamp,
    };
    __set_PRIMASK(saved);
    if (error)
        return false;
    *sample = next;
    return true;
}
static bool hall_enc_close(void *handle) {
    if (!handle)
        return false;
    tHallEncDev *inst = (tHallEncDev *)handle;

    inst->s_open = false;
    TIM3->DIER = 0U;
    TIM3->CCER = 0U;
    TIM3->CR1 = 0U;
    return true;
}
static void hall_enc_abort(void *handle) {
    /* 无异步事务；错误通过重新 open 清除 */
}
