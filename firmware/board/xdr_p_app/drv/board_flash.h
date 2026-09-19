#ifndef XDR_BOARD_DRV_BOARD_FLASH_H
#define XDR_BOARD_DRV_BOARD_FLASH_H

#include "iap.h"

// ============================================================
// board_flash.h — 本板 Flash / IAP 出口（组装层用）
//
//   mcu_iap_config  MCU Flash 的 IAP 配置（分区表 + 平台跳转），
//                   实现见 fla_mcu.c
//   mcu_app_init    App 启动准备（复位中断向量表偏移）
//
// 介质读写本身不需要组装层参与（abs 经 flash_board 钩子直达板级）。
// ============================================================

tIAPConfig mcu_iap_config(eIAPtype type);
void mcu_app_init(void);

#endif // XDR_BOARD_DRV_BOARD_FLASH_H
