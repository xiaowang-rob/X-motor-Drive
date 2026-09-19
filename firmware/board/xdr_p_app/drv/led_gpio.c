// ============================================================
// led_gpio.c — 板载 GPIO LED 驱动（板级，直连 HAL）
//
// 板上两颗普通 LED（CAN 状态灯 / 编码器状态灯）的 tLedDriverOps 实现。
// 实例形态：文件内静态 handle 数组，每个实例含
//   - ops 指针、配置（端口 / 引脚 / 触发电平）
// 引脚映射与极性只出现在本文件。
// ============================================================
#include "led_drivers.h"

#include "gpio.h"

// ---------- 本板配置 ----------
#define LED_DRV_NUM 2U // 板上普通 LED 数量

#define LED_0_GPIO_PORT GPIOD
#define LED_0_GPIO_PIN GPIO_PIN_2
#define LED_1_GPIO_PORT GPIOB
#define LED_1_GPIO_PIN GPIO_PIN_3

// ---------- 实例 handle ----------
typedef struct
{
    GPIO_TypeDef *port; // 配置：端口
    uint16_t pin;       // 配置：引脚
    bool active_level;  // 配置：触发电平（true 高电平点亮）
} tLedGpio;

static bool led_drv_init(LedHandle h);
static void led_drv_set(LedHandle h, bool active);
static void led_drv_toggle(LedHandle h);

const tLedDriverOps led_drv_ops = {
    .init = led_drv_init,
    .set = led_drv_set,
    .toggle = led_drv_toggle,
};

// 静态实例（低电平点亮）
static tLedGpio s_leds[LED_DRV_NUM] = {
    {.port = LED_0_GPIO_PORT, .pin = LED_0_GPIO_PIN, .active_level = false},
    {.port = LED_1_GPIO_PORT, .pin = LED_1_GPIO_PIN, .active_level = false},
};

LedHandle led_get_handle(uint8_t idx)
{
    if (idx >= LED_DRV_NUM)
        return NULL;
    return (LedHandle)&s_leds[idx];
}

// ---- ops 实现 ----

static bool led_drv_init(LedHandle h)
{
    if (!h)
        return false;
    return true; // GPIO 已由 CubeMX 初始化为输出
}

static void led_drv_set(LedHandle h, bool active)
{
    tLedGpio *inst = (tLedGpio *)h;
    if (!inst)
        return;
    GPIO_PinState level = (active == inst->active_level) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(inst->port, inst->pin, level);
}

static void led_drv_toggle(LedHandle h)
{
    tLedGpio *inst = (tLedGpio *)h;
    if (!inst)
        return;
    HAL_GPIO_TogglePin(inst->port, inst->pin);
}
