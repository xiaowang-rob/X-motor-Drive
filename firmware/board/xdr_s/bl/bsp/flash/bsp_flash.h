#ifndef __BSP_FLASH_H
#define __BSP_FLASH_H

#include "bl_iap.h"
#include "storage.h"

// ============================================================
//  Flash 介质驱动
//
// 实现 abs/flash.h 的 tFlashOps，并导出驱动实例供装配层挂钩；
// ============================================================

// 驱动实例
const tStorageDevHandle *Get_MCU_FlashHandle(void);

// 驱动 ops（abs/flash.h 的 tFlashOps）
extern const tStorageOps fla_mcu_ops;

// MCU IAP 应用初始化函数
// 在 IAP 应用启动时调用，用于初始化 MCU IAP 应用环境
void mcu_iap_app_init(void);
// MCU IAP app 分区
extern const tBlAppPartition g_app_partition;
#endif // __BSP_FLASH_H
