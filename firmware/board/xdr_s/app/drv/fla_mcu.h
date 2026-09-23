#ifndef __FLA_MCU_H
#define __FLA_MCU_H

#include "flash.h"
#include "iap.h"

// ============================================================
// fla_mcu.h — MCU 内部 Flash 介质驱动（板级）
//
// 实现 abs/flash.h 的 tFlashOps，并导出驱动实例供装配层挂钩；
// 同时提供 IAP 出口（分区表符号 + 平台跳转，见 board_flash.h）。
// ============================================================

// 驱动实例（布局私有：只暴露符号，装配层仅取地址）
typedef struct tFlaMcu tFlaMcu;
extern tFlaMcu g_fla_mcu;

// 驱动 ops（abs/flash.h 的 tFlashOps）
extern const tFlashOps fla_mcu_ops;

extern const tIAPPartition g_bl_parts;
extern const tIAPPartition g_app_parts;

void mcu_iap_app_init(void);
bool mcu_iap_jump_bl(void);
bool mcu_iap_jump_app(void);

#endif // __FLA_MCU_H
