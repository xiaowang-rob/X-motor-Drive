// ============================================================
// led_gpio.c — 板载 GPIO LED 驱动
// ============================================================
#include "led_gpio.h"

#include "gpio.h"

// ---------- 本板配置 ----------
#define LED_0_GPIO_PORT GPIOA
#define LED_0_GPIO_PIN GPIO_PIN_4
#define LED_0_ACTIVE_LEVEL false // 低电平点亮

#define LED_1_GPIO_PORT GPIOA
#define LED_1_GPIO_PIN GPIO_PIN_5
#define LED_1_ACTIVE_LEVEL false // 低电平点亮

// ---------- 实例 ----------
struct tLedGpio
{
    GPIO_TypeDef *port; // 端口
    uint16_t pin;       // 引脚
    bool active_level;  // 触发电平（true 高电平点亮）
};

tLedGpio g_led_gpio_0 = {.port = LED_0_GPIO_PORT, .pin = LED_0_GPIO_PIN, .active_level = LED_0_ACTIVE_LEVEL};
tLedGpio g_led_gpio_1 = {.port = LED_1_GPIO_PORT, .pin = LED_1_GPIO_PIN, .active_level = LED_1_ACTIVE_LEVEL};

// ---------- 驱动接口（tLedOps） ----------

static bool led_gpio_open(void *handle)
{
    const tLedGpio *led = (const tLedGpio *)handle;
    if (!led)
        return false;
    return true;
}

static void led_gpio_set(void *handle, bool active)
{
    const tLedGpio *led = (const tLedGpio *)handle;
    if (!led)
        return;
    GPIO_PinState level = (active == led->active_level) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(led->port, led->pin, level);
}

static void led_gpio_toggle(void *handle)
{
    const tLedGpio *led = (const tLedGpio *)handle;
    if (!led)
        return;
    HAL_GPIO_TogglePin(led->port, led->pin);
}

// ---- 驱动出口 ----
const tLedOps led_gpio_ops = {
    .open = led_gpio_open,
    .set = led_gpio_set,
    .toggle = led_gpio_toggle,
};
