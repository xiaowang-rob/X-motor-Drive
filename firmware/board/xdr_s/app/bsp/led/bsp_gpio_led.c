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

tGpioLed gpio_led = {.port = GPIOB, .pin = GPIO_PIN_11, .active_level = false};
// ---------- 驱动接口（tLedOps） ----------

static bool led_gpio_open(void *handle) {
    const tGpioLed *led = (const tGpioLed *)handle;
    if (!led)
        return false;
    return true;
}

static void led_gpio_set(void *handle, tRGBColor color, uint8_t brightness) {
    (void)color;
    bool active = brightness != 0U;
    const tGpioLed *led = (const tGpioLed *)handle;
    if (!led)
        return;
    if (active == led->active_level)
        LL_GPIO_SetOutputPin(led->port, led->pin);
    else
        LL_GPIO_ResetOutputPin(led->port, led->pin);
}

// ---- 驱动出口 ----
const tLedOps led_gpio_ops = {
    .open = led_gpio_open,
    .set = led_gpio_set,
};

static const tLedDevHandle s_dev_handle = {
    .ops = &led_gpio_ops,
    .dev = &gpio_led,
    .type = LED_TYPE_LED,
};

const tLedDevHandle *Get_Gpio_LEDHandle(void) { return &s_dev_handle; }
