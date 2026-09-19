// ============================================================
// board_flash.c — Flash 介质板级钩子的分派层
//
// 实现 app/abs/flash_board.h：按 eFlashDev 把调用转给具体介质驱动
// （fla_mcu.c / fla_w25qxx.c）——直接函数调用，无 ops 表。
// ============================================================
#include "flash_board.h"

#include "fla_mcu.h"
#include "fla_w25qxx.h"

bool flash_board_open(eFlashDev dev)
{
    switch (dev)
    {
    case FLASH_DEV_MCU:
        return fla_mcu_open();
    case FLASH_DEV_W25QXX:
        return fla_w25_open();
    default:
        return false;
    }
}

bool flash_board_read(eFlashDev dev, uint32_t addr, uint8_t *data, uint32_t len)
{
    switch (dev)
    {
    case FLASH_DEV_MCU:
        return fla_mcu_read(addr, data, len);
    case FLASH_DEV_W25QXX:
        return fla_w25_read(addr, data, len);
    default:
        return false;
    }
}

bool flash_board_write(eFlashDev dev, uint32_t addr, const uint8_t *data, uint32_t len)
{
    switch (dev)
    {
    case FLASH_DEV_MCU:
        return fla_mcu_write(addr, data, len);
    case FLASH_DEV_W25QXX:
        return fla_w25_write(addr, data, len);
    default:
        return false;
    }
}

bool flash_board_erase_addr(eFlashDev dev, uint32_t addr, uint32_t len)
{
    switch (dev)
    {
    case FLASH_DEV_MCU:
        return fla_mcu_erase_addr(addr, len);
    case FLASH_DEV_W25QXX:
        return fla_w25_erase_addr(addr, len);
    default:
        return false;
    }
}

bool flash_board_erase_sector(eFlashDev dev, uint8_t sec_id)
{
    switch (dev)
    {
    case FLASH_DEV_MCU:
        return fla_mcu_erase_sector(sec_id);
    case FLASH_DEV_W25QXX:
        return fla_w25_erase_sector(sec_id);
    default:
        return false;
    }
}

uint8_t flash_board_sector_count(eFlashDev dev)
{
    switch (dev)
    {
    case FLASH_DEV_MCU:
        return fla_mcu_sector_count();
    case FLASH_DEV_W25QXX:
        return fla_w25_sector_count();
    default:
        return 0U;
    }
}

uint32_t flash_board_sector_addr(eFlashDev dev, uint8_t sec_id)
{
    switch (dev)
    {
    case FLASH_DEV_MCU:
        return fla_mcu_sector_addr(sec_id);
    case FLASH_DEV_W25QXX:
        return fla_w25_sector_addr(sec_id);
    default:
        return 0U;
    }
}

uint32_t flash_board_sector_size(eFlashDev dev, uint8_t sec_id)
{
    switch (dev)
    {
    case FLASH_DEV_MCU:
        return fla_mcu_sector_size(sec_id);
    case FLASH_DEV_W25QXX:
        return fla_w25_sector_size(sec_id);
    default:
        return 0U;
    }
}
