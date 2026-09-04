// ============================================================
// led_drv.c — 板载 GPIO LED 驱动（usr/drv，v2 直连版）
//
// 板上两颗普通 LED（CAN 状态灯 / 编码器状态灯）的 tLedDriverOps 实现。
// 引脚映射与极性只出现在本文件（经 platform.h 宏）。
// ============================================================

#include "usr/abs/led.h"

#include "platform.h"
#include "led_drivers.h"

#define LED_DRV_NUM 2U

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    bool active_low;
} tLedDrvRes;

// idx0 = CAN 接收灯，idx1 = 编码器灯（低电平点亮）
static tLedDrvRes g_leds[LED_DRV_NUM] = {
    {LED_CANrx_GPIOx, LED_CANrx_GPIOx_PIN, true},
    {LED_ENCODER_GPIOx, LED_ENCODER_GPIOx_PIN, true},
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
    GPIO_PinState level = (active == r->active_low) ? GPIO_PIN_RESET : GPIO_PIN_SET;
    HAL_GPIO_WritePin(r->port, r->pin, level);
}

static void led_drv_toggle(LedHandle h)
{
    if (!h)
        return;
    tLedDrvRes *r = (tLedDrvRes *)h;
    HAL_GPIO_TogglePin(r->port, r->pin);
}

static const tLedDriverOps g_led_drv_ops = {
    .init = led_drv_init,
    .set = led_drv_set,
    .toggle = led_drv_toggle,
};

const tLedDriverOps *led_drv_ops(void)
{
    return &g_led_drv_ops;
}

LedHandle led_drv_handle(uint8_t idx)
{
    if (idx >= LED_DRV_NUM)
        return NULL;
    return (LedHandle)&g_leds[idx];
}
