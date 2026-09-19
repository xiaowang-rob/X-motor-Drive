#ifndef XDR_BOARD_DRV_FLA_MCU_H
#define XDR_BOARD_DRV_FLA_MCU_H

#include "iap.h"

// MCU 内部 Flash —— 板级钩子的具体实现之一
bool fla_mcu_open(void);
bool fla_mcu_read(uint32_t addr, uint8_t *data, uint32_t len);
bool fla_mcu_write(uint32_t addr, const uint8_t *data, uint32_t len);
bool fla_mcu_erase_addr(uint32_t addr, uint32_t len);
bool fla_mcu_erase_sector(uint8_t sec_id);
uint8_t fla_mcu_sector_count(void);
uint32_t fla_mcu_sector_addr(uint8_t sec_id);
uint32_t fla_mcu_sector_size(uint8_t sec_id);

#endif // XDR_BOARD_DRV_FLA_MCU_H
