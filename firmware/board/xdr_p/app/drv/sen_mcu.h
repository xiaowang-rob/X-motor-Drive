#ifndef __SEN_MCU_H
#define __SEN_MCU_H

#include "sense.h"

// ============================================================
// sen_mcu.h — 板载 ADC 采样驱动（板级）
//
//   ADC1：12bit×3（三相电流），TIM8_TRGO 触发 + DMA 持续
//   ADC2：8bit×2（Vbus/温度），软件触发，每轮 2 转换
//
// 实现 abs/sense.h 的 tSenseOps，并导出驱动实例供装配层挂钩。
// ============================================================

// 驱动实例（布局私有：只暴露符号，装配层仅取地址）
typedef struct tSenseAdc tSenseAdc;
extern tSenseAdc g_sen_mcu;

// 驱动 ops（abs/sense.h 的 tSenseOps）
extern const tSenseOps sen_mcu_ops;

#endif // __SEN_MCU_H
