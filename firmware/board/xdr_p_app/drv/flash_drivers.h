#ifndef __FLASH_DRIVERS_H
#define __FLASH_DRIVERS_H

#include "flash.h"
#include "iap.h"

// ============================================================
// flash_drivers.h — Flash 芯片驱动统一出口（usr/drv，v2 直连版）
// ============================================================

// ---- W25Q128（SPI NOR，8bit / 16MB / 4KB 扇区） ----
extern const tFlashDriverOps w25qxx_driver_ops;

// 取外部 Flash 实例句柄（静态实例，见 fla_w25qxx.c）
FlashChipHandle w25qxx_get_handle(void);

// ---- MCU Flash ----
extern const tFlashDriverOps mcu_flash_driver_ops;

// 取 MCU Flash 实例句柄（静态实例，见 fla_mcu.c）
FlashChipHandle mcu_flash_get_handle(void);

// 取 MCU Flash 的 IAP 配置（复用上面的介质 ops + 分区表 + 平台跳转）
tIAPConfig mcu_iap_config(eIAPtype type);
void mcu_app_init(void);

#endif // __FLASH_DRIVERS_H
