#include "bsp_enc_tim.h"

#include "gpio.h"
#include "tim.h"

static void (*s_tim_cb)(void *);
static void *s_tim_cb_ctx;

// 向量表中的 TIM3 弱符号由此覆盖；生成文件未定义 TIM3_IRQHandler。
void TIM3_IRQHandler(void) {
    if (s_tim_cb)
        s_tim_cb(s_tim_cb_ctx);
}

void bsp_tim_register_cb(void (*cb)(void *), void *handle) {
    s_tim_cb = cb;
    s_tim_cb_ctx = cb ? handle : NULL;
}

void bsp_tim_enc_init(void) {
    // AF2 是定时器输入路由；虽配置推挽，CCxS=输入时不会驱动传感器。
    const uint32_t pins[] = {LL_GPIO_PIN_6, LL_GPIO_PIN_7};
    for (unsigned i = 0; i < 2; ++i) {
        LL_GPIO_SetAFPin_0_7(GPIOA, pins[i], LL_GPIO_AF_2);
        LL_GPIO_SetPinPull(GPIOA, pins[i], LL_GPIO_PULL_NO);
        LL_GPIO_SetPinOutputType(GPIOA, pins[i], LL_GPIO_OUTPUT_PUSHPULL);
        LL_GPIO_SetPinSpeed(GPIOA, pins[i], LL_GPIO_SPEED_FREQ_LOW);
        LL_GPIO_SetPinMode(GPIOA, pins[i], LL_GPIO_MODE_ALTERNATE);
    }
    LL_GPIO_SetAFPin_0_7(GPIOB, LL_GPIO_PIN_0, LL_GPIO_AF_2);
    LL_GPIO_SetPinPull(GPIOB, LL_GPIO_PIN_0, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinOutputType(GPIOB, LL_GPIO_PIN_0, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(GPIOB, LL_GPIO_PIN_0, LL_GPIO_SPEED_FREQ_LOW);
    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_0, LL_GPIO_MODE_ALTERNATE);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);
}

void bsp_tim_enc_configure(const tTimEncConfig *cfg) {
    if (!cfg)
        return;
    TIM3->CR1 = 0U;
    TIM3->PSC = cfg->psc;
    TIM3->ARR = cfg->arr;
    TIM3->CR2 = cfg->cr2;
    TIM3->CCMR1 = cfg->ccmr1;
    TIM3->CCMR2 = cfg->ccmr2;
    TIM3->SMCR = cfg->smcr;
    TIM3->CCER = cfg->ccer;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->CNT = 0U;
    TIM3->SR = 0U;
    TIM3->DIER = cfg->dier;
    TIM3->CR1 = cfg->cr1;
}

uint32_t bsp_tim_enc_status(void) { return TIM3->SR; }

void bsp_tim_enc_clear_status(uint32_t mask) {
    // RC_W0：仅清除调用方看见的标志，避免误清新到达的事件。
    TIM3->SR = ~mask;
}

uint32_t bsp_tim_enc_count(void) { return TIM3->CNT; }

void bsp_tim_enc_set_count(uint32_t count) { TIM3->CNT = count; }

uint32_t bsp_tim_enc_capture(uint32_t channel) {
    switch (channel) {
    case 1U:
        return TIM3->CCR1;
    case 2U:
        return TIM3->CCR2;
    case 3U:
        return TIM3->CCR3;
    case 4U:
        return TIM3->CCR4;
    default:
        return 0U;
    }
}

void bsp_tim_enc_stop(void) {
    TIM3->DIER = 0U;
    TIM3->CCER = 0U;
    TIM3->CR1 = 0U;
}
