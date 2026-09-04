#ifndef __SENSE_DRIVERS_H
#define __SENSE_DRIVERS_H

#include "usr/abs/sense.h" // tSampleIf

// ============================================================
// sense_drivers.h — 板采样驱动统一出口（usr/drv，v2 直连版）
//
// 实现 tSampleIf（ADC1 电流 + ADC2 Vbus/温度），直调本板 HAL
// （经 platform.h）；中断 HAL_ADC_ConvCpltCallback 由本模块唯一持有。
// ============================================================

const tSampleIf *sense_drv_get(void);

#endif // __SENSE_DRIVERS_H
