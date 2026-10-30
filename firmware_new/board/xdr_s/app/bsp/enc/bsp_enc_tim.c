#include "bsp_enc_tim.h"

#include "gpio.h"
#include "tim.h"

void (*tim_cb)(void *handle);
void *tim_cb_ctx;
// 向量表中的 TIM3 弱符号由此覆盖；生成文件未定义 TIM3_Handler。
void TIM3_IRQHandler(void) {
    if (!tim_cb)
        tim_cb(tim_cb_ctx);
}

void bsp_tim_register_cb(void *cb, void *handle) {
    tim_cb = cb;
    tim_cb_ctx = handle;
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
