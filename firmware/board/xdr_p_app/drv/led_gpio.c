// ============================================================
// led_drv.c — 板载 GPIO LED 驱动（usr/drv，v2 直连版）
//
// 板上两颗普通 LED（CAN 状态灯 / 编码器状态灯）的 tLedDriverOps 实现。
// 引脚映射与极性只出现在本文件（经 platform.h 宏）。
// ============================================================
#include "led_drivers.h"
#include "gpio.h"

#define LED_DRV_NUM 2U

#define LED_0_GPIOx GPIOD
#define LED_0_GPIOx_PIN GPIO_PIN_2
#define LED_1_GPIOx GPIOB
#define LED_1_GPIOx_PIN GPIO_PIN_3
typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    bool active_level; // 触发电平 true 高电平 / false 低电平
} tLedDrvRes;

// （低电平点亮）
static tLedDrvRes g_leds[LED_DRV_NUM] = {
    {LED_0_GPIOx, LED_0_GPIOx_PIN, false},
    {LED_1_GPIOx, LED_1_GPIOx_PIN, false},
};

static bool led_drv_init(LedHandle h)
{
    if (!h)
        return false;
    return true; // GPIO 已由 CubeMX 初始化为输出
}

static void led_drv_set(LedHandle h, bool active)
{
    if (!h)
        return;
    tLedDrvRes *r = (tLedDrvRes *)h;
    GPIO_PinState level = active == r->active_level ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(r->port, r->pin, level);
}

static void led_drv_toggle(LedHandle h)
{
    if (!h)
        return;
    tLedDrvRes *r = (tLedDrvRes *)h;
    HAL_GPIO_TogglePin(r->port, r->pin);
}

const tLedDriverOps led_drv_ops = {
    .init = led_drv_init,
    .set = led_drv_set,
    .toggle = led_drv_toggle,
};

LedHandle led_drv_handle(uint8_t idx)
{
    if (idx >= LED_DRV_NUM)
        return NULL;
    return (LedHandle)&g_leds[idx];
}
