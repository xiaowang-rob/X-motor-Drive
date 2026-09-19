// ============================================================
// led_gpio.c — 板载 GPIO LED 驱动（板级，直连 HAL）
//
// 实现 app/abs/led_board.h 的普通 LED 部分：
//   板上两颗 LED（CAN 状态灯 / 编码器状态灯）的 开/关/翻转。
// 实例形态：文件内 const 配置数组（端口 / 引脚 / 极性）。
// 引脚映射与极性只出现在本文件。
// ============================================================
#include "led_board.h"

#include "gpio.h"

// ---------- 本板配置 ----------
#define LED_DRV_NUM 2U // 板上普通 LED 数量

#define LED_0_GPIO_PORT GPIOD
#define LED_0_GPIO_PIN GPIO_PIN_2
#define LED_1_GPIO_PORT GPIOB
#define LED_1_GPIO_PIN GPIO_PIN_3

// ---------- 实例配置（const，驻 Flash） ----------
typedef struct
{
    GPIO_TypeDef *port; // 端口
    uint16_t pin;       // 引脚
    bool active_level;  // 触发电平（true 高电平点亮）
} tLedGpio;

static const tLedGpio s_leds[LED_DRV_NUM] = {
    {.port = LED_0_GPIO_PORT, .pin = LED_0_GPIO_PIN, .active_level = false},
    {.port = LED_1_GPIO_PORT, .pin = LED_1_GPIO_PIN, .active_level = false},
};

static bool led_idx_ok(uint8_t idx)
{
    return idx < LED_DRV_NUM;
}

// ---------- 板级钩子实现 ----------

bool led_board_open(uint8_t idx)
{
    if (!led_idx_ok(idx))
        return false;
    return true; // GPIO 已由 CubeMX 初始化为输出
}

void led_board_set(uint8_t idx, bool active)
{
    if (!led_idx_ok(idx))
        return;
    const tLedGpio *led = &s_leds[idx];
    GPIO_PinState level = (active == led->active_level) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(led->port, led->pin, level);
}

void led_board_toggle(uint8_t idx)
{
    if (!led_idx_ok(idx))
        return;
    HAL_GPIO_TogglePin(s_leds[idx].port, s_leds[idx].pin);
}
