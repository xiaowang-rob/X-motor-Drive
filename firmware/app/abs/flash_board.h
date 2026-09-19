#ifndef XDR_APP_ABS_FLASH_BOARD_H
#define XDR_APP_ABS_FLASH_BOARD_H

#include "flash.h"

// ============================================================
// flash_board.h — Flash 介质板级钩子契约（abs 声明 / 板级实现）
//
// 实现 = board/<B>/drv/board_flash.c（按 eFlashDev 分派到
//        fla_mcu.c / fla_w25qxx.c）。
//
// 语义：
//   open          介质初始化（含连接校验，如 JEDEC ID）；返回前资源须可用
//   read/write    连续读写（write 内部处理页/对齐，调用方保证目标已擦除）
//   erase_addr    擦除从 addr 起覆盖 len 的区域（按扇区粒度向上取整）
//   erase_sector  擦除指定用户扇区（编号）
//   sector_*      用户扇区几何
// ============================================================

bool flash_board_open(eFlashDev dev);
bool flash_board_read(eFlashDev dev, uint32_t addr, uint8_t *data, uint32_t len);
bool flash_board_write(eFlashDev dev, uint32_t addr, const uint8_t *data, uint32_t len);
bool flash_board_erase_addr(eFlashDev dev, uint32_t addr, uint32_t len);
bool flash_board_erase_sector(eFlashDev dev, uint8_t sec_id);

uint8_t flash_board_sector_count(eFlashDev dev);
uint32_t flash_board_sector_addr(eFlashDev dev, uint8_t sec_id);
uint32_t flash_board_sector_size(eFlashDev dev, uint8_t sec_id);

#endif // XDR_APP_ABS_FLASH_BOARD_H
