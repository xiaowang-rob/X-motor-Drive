#ifndef __BSP_ENC_ADC_H
#define __BSP_ENC_ADC_H

#include <stdbool.h>
#include <stdint.h>

// sin/cos 编码器模拟采样链路层：只负责引脚/ADC 前端配置与一次采样，
// 不持有编码器语义。本层不 include encoder.h / bsp_enc.h。

// 将 PA6/PA7 切为模拟输入；采样链未接入 ADC 时仍返回 true（仅完成前端配置）。
bool bsp_enc_adc_sincos_open(void);

// 读取一路 sin/cos 原始码值。真实 ADC/DMA 采样链打通前返回 false，
// 避免上层把未初始化缓冲当作有效角度。
bool bsp_enc_adc_sincos_read(uint16_t *sin_raw, uint16_t *cos_raw);

#endif // __BSP_ENC_ADC_H
