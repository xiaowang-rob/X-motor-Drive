#ifndef __SEN_MCU_H
#define __SEN_MCU_H

#include "sense.h"

// ============================================================
// sen_mcu.h — 板载 ADC 采样驱动
// ============================================================

// 驱动实例
typedef struct tSenseAdc tSenseAdc;
extern tSenseAdc g_sen_mcu;

// 驱动 ops
extern const tSenseOps sen_mcu_ops;

#endif // __SEN_MCU_H
