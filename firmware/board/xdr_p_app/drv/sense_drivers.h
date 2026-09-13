#ifndef __SENSE_DRIVERS_H
#define __SENSE_DRIVERS_H

#include "sense.h"

// ============================================================
// sense_drivers.h — 本板采样驱动出口
//
// 采样前端以"文件内静态 handle 实例"存在，本头只暴露 ops 与取实例函数。
// ============================================================

extern const tSampleMcuOps mcu_adc_ops;

// 取采样实例句柄（静态实例，见 sen_mcu.c）
SampleHandle sense_get_handle(void);

#endif // __SENSE_DRIVERS_H
