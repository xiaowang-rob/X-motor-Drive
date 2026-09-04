#ifndef __MCU_FLASH_DRV_H
#define __MCU_FLASH_DRV_H

#include "usr/abs/flash.h"

// ============================================================
// mcu_flash_drv.h — 内部 MCU Flash 介质驱动（usr/drv，v2 直连版）
//
// 实现 tFlashDriverOps，擦写按地址寻址（F405 混合扇区几何见 .c）。
// 几何契约：get_sector_size 返回 128KB（擦除单元 attach 限制）；
// erase() 内部按真实扇区几何逐扇区擦除，故 BL/APP/IAP 区无此限制。
// ============================================================

FlashChipHandle mcu_flash_create(void);
void mcu_flash_destroy(FlashChipHandle h);
extern const tFlashDriverOps mcu_flash_driver_ops;

#endif // __MCU_FLASH_DRV_H
