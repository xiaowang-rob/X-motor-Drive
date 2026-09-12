#ifndef __FLASH_DRIVERS_H
#define __FLASH_DRIVERS_H

#include "flash.h"
#include "iap.h"

// ============================================================
// flash_drivers.h — Flash 芯片驱动统一出口（usr/drv，v2 直连版）
// ============================================================

// ---- W25Q128（SPI NOR，8bit / 16MB / 4KB 扇区） ----
FlashChipHandle w25qxx_create(void);
void w25qxx_destroy(FlashChipHandle h);
extern const tFlashDriverOps w25qxx_driver_ops;

// ---- MCU Flash ----
FlashChipHandle mcu_flash_create(void);
void mcu_flash_destroy(FlashChipHandle h);
extern const tFlashDriverOps mcu_flash_driver_ops;
extern const tIAPDriverOps mcu_iap_driver_ops;
#endif // __FLASH_DRIVERS_H
