#ifndef __BSP_ADC_H
#define __BSP_ADC_H

#include "sample.h"

#include "adc.h"
#include "tim.h"

// ADC 采样配置
#define CUR_ADC ADC1
#define CUR_ADC_DMA DMA1
#define CUR_ADC_DMA_CH LL_DMA_CHANNEL_1
#define CUR_ADC_SEQ_LEN 3U
#define CUR_ADC_TIM TIM1
#define CUR_ADC_TIM_ARR (CUR_ADC_TIM->ARR) // PWM 周期计数值
// 设置默认采样点
#define CUR_SET_DEF_SP()                          \
    {                                             \
        CUR_ADC_TIM->CCR4 = CUR_ADC_TIM->ARR - 1; \
    }
// 设置采样点
#define CUR_SET_SP(tic)          \
    {                            \
        CUR_ADC_TIM->CCR4 = tic; \
    }

#define SENSE_CUR_GAIN 100.0f          // 电流采样放大倍数
#define ADC_T_SAMPLE_CUR (1 / 1000000) // 采样时间
#define CURRENT_U_CH 0                 // 电流通道
#define CURRENT_V_CH 1                 // 电流通道
#define CURRENT_W_CH 2                 // 电流通道

#define VT_ADC ADC2
#define VT_ADC_DMA DMA1
#define VT_ADC_DMA_CH LL_DMA_CHANNEL_4
#define VT_ADC_SEQ_LEN 2U
#define VT_ADC_TIM TIM2
#define SENSE_VBUS_GAIN 16.0f // 母线分压比

#define VBUS_CH 0 // 母线电压通道
#define TEMP_CH 1 // 温度通道

// 获取驱动句柄
const tSampleDevHandle *Get_MCU_ADCHandle(void);
// 驱动 ops
extern const tSampleOps sen_mcu_ops;

#endif // __BSP_ADC_H
