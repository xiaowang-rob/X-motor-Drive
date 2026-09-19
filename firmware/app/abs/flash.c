// ============================================================
// flash.c — 日志式存储单元业务（abs，纯逻辑）
//
// 在板级介质（flash_board）之上提供"顺序追加记录"的单元管理：
//   - 单元 = 介质的一个擦除单元（扇区）
//   - 写入：追加到 free 偏移（写满才擦除 → 磨损友好）
//   - 空闲边界探测：二分读 2 字节，全 0xFF = 未写（重启后恢复续写位置）
//
// 注：记录格式/校验由上层定义（参数、日志等），本层不解析内容。
//     设备状态由本层 dstate 维护，板级只返回 bool 成败。
// ============================================================

#include "flash.h"

#include "flash_board.h"

// 空闲探测读窗口（字节）
#define FLASH_FREE_SCAN_WIN 2U

// 探测单元空闲边界：二分查找"首个未写偏移"
static bool flash_scan_free(tFlash *s, tFlashUnit *unit)
{
    uint8_t buf[FLASH_FREE_SCAN_WIN];

    // 先看单元首字节：全 FF 视为空单元
    if (!flash_board_read(s->dev, unit->base_addr, buf, FLASH_FREE_SCAN_WIN))
        return false;
    bool all_ff = (buf[0] == 0xFFU && buf[1] == 0xFFU);
    if (all_ff)
    {
        // 可能空单元，也可能写满后尾部对齐 FF —— 由尾探测区分
        if (!flash_board_read(s->dev, unit->base_addr + unit->size - FLASH_FREE_SCAN_WIN,
                              buf, FLASH_FREE_SCAN_WIN))
            return false;
        if (buf[0] == 0xFFU && buf[1] == 0xFFU)
        {
            unit->free_addr = 0U; // 首尾皆 FF：视为空单元
            return true;
        }
        unit->free_addr = unit->size; // 尾部非 FF → 写满
        return true;
    }

    // 二分：找最后一个"已写"与第一个"未写"的边界
    uint32_t lo = 0U;         // [0, lo) 已写
    uint32_t hi = unit->size; // [hi, size) 未写
    while ((hi - lo) > FLASH_FREE_SCAN_WIN)
    {
        uint32_t mid = (lo + hi) / 2U;
        uint32_t read_at = mid;
        if (read_at + FLASH_FREE_SCAN_WIN > unit->size)
            read_at = unit->size - FLASH_FREE_SCAN_WIN;

        if (!flash_board_read(s->dev, unit->base_addr + read_at, buf, FLASH_FREE_SCAN_WIN))
            return false;

        bool written = !(buf[0] == 0xFFU && buf[1] == 0xFFU);
        if (written)
            lo = (read_at + FLASH_FREE_SCAN_WIN > lo) ? read_at + FLASH_FREE_SCAN_WIN : lo + 1U;
        else
            hi = read_at;
    }
    unit->free_addr = lo;
    return true;
}

bool flash_init(tFlash *s, eFlashDev dev)
{
    if (!s || (unsigned)dev >= (unsigned)FLASH_DEV_NUM)
        return false;

    s->dev = dev;
    s->dstate = DEV_OFFLINE;

    s->usr_sector_count = flash_board_sector_count(dev);
    s->usr_sector_bit_status = 0U; // 全部未注册
    s->usr_sector_free = (uint8_t)s->usr_sector_count;

    if (!flash_board_open(dev))
    {
        s->dstate = DEV_RUN_ERROR;
        return false;
    }

    s->dstate = DEV_ONLINE;
    return true;
}

// 注册一个日志式存储单元（unit 由调用方提供）
bool flash_unit_register(tFlash *s, tFlashUnit *unit)
{
    if (!s || !unit)
        return false;
    if (s->usr_sector_free == 0U)
        return false;

    for (uint32_t i = 0U; i < s->usr_sector_count; i++)
    {
        if (0U == (s->usr_sector_bit_status & (uint16_t)(1U << i)))
        {
            unit->id = (uint8_t)i;
            unit->base_addr = flash_board_sector_addr(s->dev, (uint8_t)i);
            unit->size = flash_board_sector_size(s->dev, (uint8_t)i);
            if (!flash_scan_free(s, unit))
                return false;

            s->usr_sector_bit_status |= (uint16_t)(1U << i);
            s->usr_sector_free--;
            return true;
        }
    }
    return false;
}

void flash_unit_unregister(tFlash *s, tFlashUnit *unit)
{
    if (!s || !unit)
        return;
    s->usr_sector_bit_status &= (uint16_t)~(1U << unit->id);
    s->usr_sector_free++;
}

// 追加写入一条记录（自动落在 free_addr）；空间不足先擦除再从头写
bool flash_unit_append(tFlash *s, tFlashUnit *unit, const uint8_t *data, uint32_t len)
{
    if (!s || !unit || !data || len == 0U)
        return false;

    if ((unit->free_addr + len) > unit->size)
    {
        if (!flash_unit_erase(s, unit))
            return false;
    }

    uint32_t addr = unit->base_addr + unit->free_addr;
    if (!flash_board_write(s->dev, addr, data, len))
    {
        s->dstate = DEV_RUN_ERROR;
        return false;
    }

    unit->free_addr += len; // 推进写位置
    s->dstate = DEV_RUNNING;
    return true;
}

// 读上一条记录数据
bool flash_unit_read(tFlash *s, tFlashUnit *unit, uint8_t *data, uint32_t len)
{
    if (!s || !unit || !data || len == 0U)
        return false;
    if (unit->free_addr < len)
        return false; // 无记录

    uint32_t addr = unit->base_addr + unit->free_addr - len;
    return flash_board_read(s->dev, addr, data, len);
}

// 擦除整个单元并复位写位置
bool flash_unit_erase(tFlash *s, tFlashUnit *unit)
{
    if (!s || !unit)
        return false;

    if (!flash_board_erase_sector(s->dev, unit->id))
    {
        s->dstate = DEV_RUN_ERROR;
        return false;
    }
    unit->free_addr = 0U;
    return true;
}
