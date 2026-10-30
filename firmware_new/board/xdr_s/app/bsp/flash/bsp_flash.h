#ifndef __BSP_FLASH_H
#define __BSP_FLASH_H

#include "flash.h"
#include "iap.h"

// ============================================================
//  Flash 介质驱动
//
// 实现 abs/flash.h 的 tFlashOps，并导出驱动实例供装配层挂钩；
// ============================================================

// 驱动实例
const tFlashDevHandle *Get_MCU_FlashHandle(void);

// 驱动 ops（abs/flash.h 的 tFlashOps）
extern const tFlashOps fla_mcu_ops;

extern const tIAPPartition g_bl_parts;
extern const tIAPPartition g_app_parts;

void mcu_iap_app_init(void);
bool mcu_iap_jump_bl(void);
bool mcu_iap_jump_app(void);

#endif // __BSP_FLASH_H
