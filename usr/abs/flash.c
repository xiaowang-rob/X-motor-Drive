// ============================================================
// flash.c — 日志式存储单元业务（usr/abs，纯逻辑）
//
// 在 tFlashDriverOps 介质之上提供一个"顺序追加记录"的单元管理：
//   - 单元 = 介质的一个擦除单元（扇区）
//   - 写入：追加到 free 偏移（写满才擦除 → 磨损友好）
//   - 空闲边界探测：二分读 2 字节，全 0xFF = 未写（重启后恢复续写位置）
//
// 注：记录格式/校验由上层定义（参数、日志等），本层不解析内容。
// ============================================================

#include "flash.h"

// 空闲探测读窗口（字节）
#define FLASH_FREE_SCAN_WIN 2U

// 探测单元空闲边界：二分查找"首个未写偏移"
// 不变式：探测区间 [lo, hi)，lo 指向已写区末尾方向、hi 指向未写区
static bool flash_scan_free(tFlash *s, tFlashUnit *unit)
{
    uint8_t buf[FLASH_FREE_SCAN_WIN];

    // 先看单元首字节：全 FF 视为空单元
    if (!s->ops->read(s->handle, unit->base_addr, buf, FLASH_FREE_SCAN_WIN))
        return false;
    bool all_ff = (buf[0] == 0xFFU && buf[1] == 0xFFU);
    if (all_ff)
    {
        // 可能空单元，也可能写满后尾部对齐 FF——由尾探测区分
        if (!s->ops->read(s->handle, unit->base_addr + unit->size - FLASH_FREE_SCAN_WIN,
                          buf, FLASH_FREE_SCAN_WIN))
            return false;
        if (buf[0] == 0xFFU && buf[1] == 0xFFU)
        {
            unit->free_addr = 0U; // 首尾皆 FF：视为空单元（记录最小长度假设 > 2 时安全）
            return true;
        }
        // 尾部非 FF → 写满
        unit->free_addr = unit->size;
        return true;
    }

    // 二分：找最后一个"已写"与第一个"未写"的边界
    uint32_t lo = 0U;         // [0, lo) 已写
    uint32_t hi = unit->size; // [hi, size) 未写
    while ((hi - lo) > FLASH_FREE_SCAN_WIN)
    {
        uint32_t mid = (lo + hi) / 2U;
        // mid 处读 2 字节；边界上可能横跨写/未写 → 读法保守处理
        uint32_t read_at = mid;
        if (read_at + FLASH_FREE_SCAN_WIN > unit->size)
            read_at = unit->size - FLASH_FREE_SCAN_WIN;

        if (!s->ops->read(s->handle, unit->base_addr + read_at, buf, FLASH_FREE_SCAN_WIN))
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

bool flash_init(tFlash *s, const tFlashDriverOps *ops, FlashChipHandle h)
{
    if (!s || !ops || !h)
        return false;

    s->ops = ops;
    s->handle = h;

    s->bl_sector_ids = s->ops->get_bl_ids(h, &s->bl_sector_count);
    s->app_sector_ids = s->ops->get_app_ids(h, &s->app_sector_count);
    s->usr_sector_ids = s->ops->get_usr_ids(h, &s->usr_sector_count);

    s->usr_sector_bit_status = 0xffff;
    s->usr_sector_bit_status << s->usr_sector_count; // 初始化：所有用户分区未注册
    s->usr_sector_free = s->usr_sector_count;

    return true; // 初始化成功
}

// 注册/注销 一个日志式存储单元
tFlashUnit *flash_unit_register(tFlash *s)
{
    if (!s)
        return NULL;
    if (0 >= s->usr_sector_free)
        return NULL;
    tFlashUnit *new_unit = (tFlashUnit *)malloc(sizeof(tFlashUnit));
    if (!new_unit)
        return NULL;
    for (uint32_t i = 0; i < s->usr_sector_count; i++)
    {
        if (0 == (s->usr_sector_bit_status & (1 << i)))
        {
            new_unit->id = i;
            new_unit->base_addr = s->ops->get_sector_addr(s->handle, s->usr_sector_ids[i]);
            new_unit->size = s->ops->get_sector_size(s->handle, s->usr_sector_ids[i]);
            if (flash_scan_free(s, new_unit))
            {
                s->usr_sector_bit_status |= (1 << i);
                s->usr_sector_free--;
                return new_unit; // 注册成功
            }
            free(new_unit);
            return NULL; // 注册失败
        }
    }
    return NULL; // 注册失败
}
void flash_unit_unregister(tFlash *s, tFlashUnit *unit)
{
    if (!s || !unit)
        return;
    s->usr_sector_bit_status &= ~(1 << unit->id);
    s->usr_sector_free++;
    free(unit);
    unit = NULL;
}

// 追加写入一条记录（自动落在 free_addr）；空间不足擦除从头开始写
bool flash_unit_append(tFlash *s, tFlashUnit *unit, const uint8_t *data, uint32_t len)
{
    if (!s || !unit || !data || len == 0U)
        return false;
    if (unit->free_addr + len > unit->size)
    {
        flash_unit_erase(s, unit);
    }
    uint32_t addr = unit->base_addr + unit->free_addr;
    return s->ops->write(s->handle, addr, data, len);
}

// 读上一条记录数据
bool flash_unit_read(tFlash *s, tFlashUnit *unit, uint8_t *data, uint32_t len)
{
    if (!s || !unit || !data || len == 0U)
        return false;
    if (unit->free_addr < len)
        return false; // 无记录
    uint32_t addr = unit->base_addr + unit->free_addr - len;
    return s->ops->read(s->handle, addr, data, len);
}

// 擦除整个单元并复位写位置
bool flash_unit_erase(tFlash *s, tFlashUnit *unit)
{
    s->ops->erase_sector(s->handle, s->usr_sector_ids[unit->id]);
    unit->free_addr = 0U;
}

// 便捷：擦写/校验整个 App / BL 区
bool flash_iap_erase_app(tFlash *s)
{
    for (uint32_t i = 0; i < s->app_sector_count; i++)
    {
        if (!s->ops->erase_sector(s->handle, s->app_sector_ids[i]))
            return false;
    }
    return true;
}
bool flash_iap_write_app(tFlash *s, uint32_t offset,
                         const uint8_t *data, uint32_t size)
{
    if (!s || !data || size == 0U)
        return false;
    uint32_t addr = offset + s->ops->get_sector_addr(s->handle, s->app_sector_ids[0]);
    return s->ops->write(s->handle, addr, data, size);
}
bool flash_iap_verify_app(tFlash *s, uint32_t offset,
                          const uint8_t *data, uint32_t size)
{
    if (!s || !data || size == 0U)
        return false;
    uint8_t verify_data;
    uint32_t addr = offset + s->ops->get_sector_addr(s->handle, s->app_sector_ids[0]);
    for (uint32_t i = 0U; i < size; i++)
    {
        if (!s->ops->read(s->handle, addr + i, &verify_data, 1))
            return false;
        if (verify_data != data[i])
            return false;
    }
    return true;
}
bool flash_iap_erase_bl(tFlash *s)
{
    for (uint32_t i = 0; i < s->bl_sector_count; i++)
    {

        if (!s->ops->erase_sector(s->handle, s->bl_sector_ids[i]))
            return false;
    }
    return true;
}

bool flash_iap_write_bl(tFlash *s, uint32_t offset,
                        const uint8_t *data, uint32_t size)
{
    if (!s || !data || size == 0U)
        return false;
    uint32_t addr = offset + s->ops->get_sector_addr(s->handle, s->bl_sector_ids[0]);
    return s->ops->write(s->handle, addr, data, size);
}
bool flash_iap_verify_bl(tFlash *s, uint32_t offset,
                         const uint8_t *data, uint32_t size)
{
    if (!s || !data || size == 0U)
        return false;
    uint8_t verify_data;
    uint32_t addr = offset + s->ops->get_sector_addr(s->handle, s->bl_sector_ids[0]);
    for (uint32_t i = 0U; i < size; i++)
    {
        if (!s->ops->read(s->handle, addr + i, &verify_data, 1))
            return false;
        if (verify_data != data[i])
            return false;
    }
    return true;
}

void flash_iap_jump_app(tFlash *s)
{
    if (!s)
        return;
    s->ops->jump_app(s->handle);
}
void flash_iap_reset(tFlash *s)
{
    if (!s)
        return;
    s->ops->jump_bl(s->handle);
}
