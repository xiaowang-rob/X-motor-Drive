#ifndef XDR_BOARD_DRV_FLA_W25QXX_H
#define XDR_BOARD_DRV_FLA_W25QXX_H

#include "flash.h"

// W25Q128 外部 SPI NOR —— 板级钩子的具体实现之一
bool fla_w25_open(void);
bool fla_w25_read(uint32_t addr, uint8_t *data, uint32_t len);
bool fla_w25_write(uint32_t addr, const uint8_t *data, uint32_t len);
bool fla_w25_erase_addr(uint32_t addr, uint32_t len);
bool fla_w25_erase_sector(uint8_t sec_id);
uint8_t fla_w25_sector_count(void);
uint32_t fla_w25_sector_addr(uint8_t sec_id);
uint32_t fla_w25_sector_size(uint8_t sec_id);

#endif // XDR_BOARD_DRV_FLA_W25QXX_H
