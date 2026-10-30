// ============================================================
// 板载 GPIO LED 驱动
// ============================================================
#include "bsp_led.h"

#include "gpio.h"

// ---------- 实例 ----------
typedef struct tGpioLed {
    GPIO_TypeDef *port; // 端口
    uint16_t pin;       // 引脚
    bool active_level;  // 触发电平（true 高电平点亮）
} tGpioLed;

tGpioLed gpio_led = {.port = GPIOA, .pin = GPIO_PIN_4, .active_level = false};
// ---------- 驱动接口（tLedOps） ----------

static bool led_gpio_open(void *handle) {
    const tGpioLed *led = (const tGpioLed *)handle;
    if (!led)
        return false;
    return true;
}

static void led_gpio_set(void *handle, bool active) {
    const tGpioLed *led = (const tGpioLed *)handle;
    if (!led)
        return;
    if (active == led->active_level)
        LL_GPIO_SetOutputPin(led->port, led->pin);
    else
        LL_GPIO_ResetOutputPin(led->port, led->pin);
}

static void led_gpio_toggle(void *handle) {
    const tGpioLed *led = (const tGpioLed *)handle;
    if (!led)
        return;
    LL_GPIO_TogglePin(led->port, led->pin);
}

// ---- 驱动出口 ----
const tLedOps led_gpio_ops = {
    .open = led_gpio_open,
    .set = led_gpio_set,
    .toggle = led_gpio_toggle,
};

static const tLedDevHandle s_dev_handle = {
    .ops = &led_gpio_ops,
    .dev = &gpio_led,
};

const tLedDevHandle *Get_Gpio_LEDHandle(void) { return &s_dev_handle; }
