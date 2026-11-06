// sin/cos 编码器模拟采样链路层：引脚/ADC 前端配置与原始码值读取。

#include "bsp_enc_adc.h"

#include "gpio.h"

bool bsp_enc_adc_sincos_open(void) {
    // 汇总层已停止 SPI/TIM3；ADC 模拟输入无需 AF 编号。
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_6, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_7, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_6, LL_GPIO_MODE_ANALOG);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_7, LL_GPIO_MODE_ANALOG);
    // TODO: 接入 ADC/DMA 采样链（通道、触发与缓冲区由板级 CubeMX 配置）。
    return true;
}

bool bsp_enc_adc_sincos_read(uint16_t *sin_raw, uint16_t *cos_raw) {
    (void)sin_raw;
    (void)cos_raw;
    // TODO: 采样链未打通前明确失败，禁止上层把 0 当作有效角度。
    return false;
}
