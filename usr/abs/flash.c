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

#include "usr/abs/flash.h"

// 空闲探测读窗口（字节）
#define FLASH_FREE_SCAN_WIN 2U

// 探测单元空闲边界：二分查找"首个未写偏移"
// 不变式：探测区间 [lo, hi)，lo 指向已写区末尾方向、hi 指向未写区
static bool flash_scan_free(tFlashStore *s, uint32_t *free_addr)
{
    const uint32_t size = s->unit.size;
    uint8_t buf[FLASH_FREE_SCAN_WIN];

    // 先看单元首字节：全 FF 视为空单元
    if (!s->ops->read(s->handle, s->unit.base_addr, buf, FLASH_FREE_SCAN_WIN))
        return false;
    bool all_ff = (buf[0] == 0xFFU && buf[1] == 0xFFU);
    if (all_ff)
    {
        // 可能空单元，也可能写满后尾部对齐 FF——由尾探测区分
        if (!s->ops->read(s->handle, s->unit.base_addr + size - FLASH_FREE_SCAN_WIN,
                          buf, FLASH_FREE_SCAN_WIN))
            return false;
        if (buf[0] == 0xFFU && buf[1] == 0xFFU)
        {
            *free_addr = 0U; // 首尾皆 FF：视为空单元（记录最小长度假设 > 2 时安全）
            return true;
        }
        // 尾部非 FF → 写满
        *free_addr = size;
        return true;
    }

    // 二分：找最后一个"已写"与第一个"未写"的边界
    uint32_t lo = 0U;      // [0, lo) 已写
    uint32_t hi = size;    // [hi, size) 未写
    while ((hi - lo) > FLASH_FREE_SCAN_WIN)
    {
        uint32_t mid = (lo + hi) / 2U;
        // mid 处读 2 字节；边界上可能横跨写/未写 → 读法保守处理
        uint32_t read_at = mid;
        if (read_at + FLASH_FREE_SCAN_WIN > size)
            read_at = size - FLASH_FREE_SCAN_WIN;

        if (!s->ops->read(s->handle, s->unit.base_addr + read_at, buf, FLASH_FREE_SCAN_WIN))
            return false;

        bool written = !(buf[0] == 0xFFU && buf[1] == 0xFFU);
        if (written)
            lo = (read_at + FLASH_FREE_SCAN_WIN > lo) ? read_at + FLASH_FREE_SCAN_WIN : lo + 1U;
        else
            hi = read_at;
    }
    *free_addr = lo;
    return true;
}

bool flash_unit_init(tFlashStore *s, const tFlashDriverOps *ops, FlashChipHandle h,
                     uint32_t base_addr)
{
    if (!s || !ops || !h)
        return false;

    uint32_t sector = ops->get_sector_size(h);
    if (sector == 0U)
        return false;

    s->ops = ops;
    s->handle = h;
    s->unit.base_addr = base_addr - (base_addr % sector); // 对齐到擦除单元
    s->unit.size = sector;
    s->unit.free_addr = 0U;

    return flash_scan_free(s, &s->unit.free_addr);
}

bool flash_unit_append(tFlashStore *s, const uint8_t *data, uint32_t len)
{
    if (!s || !data || len == 0U)
        return false;
    if (s->unit.free_addr + len > s->unit.size)
        return false; // 空间不足（上层应擦除后重写/另开单元）

    uint32_t addr = s->unit.base_addr + s->unit.free_addr;
    if (!s->ops->write(s->handle, addr, data, len))
        return false;

    s->unit.free_addr += len;
    return true;
}

bool flash_unit_read(tFlashStore *s, uint32_t offset, uint8_t *data, uint32_t len)
{
    if (!s || !data || len == 0U)
        return false;
    if (offset + len > s->unit.free_addr)
        return false; // 只允许读已写区

    return s->ops->read(s->handle, s->unit.base_addr + offset, data, len);
}

bool flash_unit_erase(tFlashStore *s)
{
    if (!s)
        return false;

    if (!s->ops->erase(s->handle, s->unit.base_addr, s->unit.size))
        return false;

    s->unit.free_addr = 0U;
    return true;
}

uint32_t flash_unit_free(tFlashStore *s)
{
    if (!s)
        return 0U;
    return s->unit.size - s->unit.free_addr;
}

bool flash_unit_is_full(tFlashStore *s)
{
    if (!s)
        return true;
    return s->unit.free_addr >= s->unit.size;
}

// ==================== IAP 固件升级编排（纯逻辑） ====================

bool flash_iap_erase(const tFlashDriverOps *ops, FlashChipHandle h, uint32_t addr, uint32_t size)
{
    if (!ops || !h || size == 0U)
        return false;
    return ops->erase(h, addr, size); // 介质驱动按擦除单元向上取整
}

bool flash_iap_write(const tFlashDriverOps *ops, FlashChipHandle h,
                     uint32_t addr, const uint8_t *data, uint32_t size)
{
    if (!ops || !h || !data || size == 0U)
        return false;
    return ops->write(h, addr, data, size);
}

bool flash_iap_verify(const tFlashDriverOps *ops, FlashChipHandle h,
                      uint32_t addr, const uint8_t *data, uint32_t size)
{
    if (!ops || !h || !data || size == 0U)
        return false;

    uint8_t buf[32];
    uint32_t done = 0U;
    while (done < size)
    {
        uint32_t n = size - done;
        if (n > sizeof(buf))
            n = sizeof(buf);
        if (!ops->read(h, addr + done, buf, n))
            return false;
        for (uint32_t i = 0U; i < n; i++)
        {
            if (buf[i] != data[done + i])
                return false;
        }
        done += n;
    }
    return true;
}

bool flash_iap_erase_app(const tFlashDriverOps *ops, FlashChipHandle h, const tFlashIAP *iap)
{
    if (!iap)
        return false;
    return flash_iap_erase(ops, h, iap->app_addr, iap->app_size);
}

bool flash_iap_write_app(const tFlashDriverOps *ops, FlashChipHandle h, const tFlashIAP *iap,
                         uint32_t offset, const uint8_t *data, uint32_t size)
{
    if (!iap || offset + size > iap->app_size)
        return false;
    return flash_iap_write(ops, h, iap->app_addr + offset, data, size);
}

bool flash_iap_verify_app(const tFlashDriverOps *ops, FlashChipHandle h, const tFlashIAP *iap,
                          uint32_t offset, const uint8_t *data, uint32_t size)
{
    if (!iap || offset + size > iap->app_size)
        return false;
    return flash_iap_verify(ops, h, iap->app_addr + offset, data, size);
}

bool flash_iap_erase_bl(const tFlashDriverOps *ops, FlashChipHandle h, const tFlashIAP *iap)
{
    if (!iap)
        return false;
    return flash_iap_erase(ops, h, iap->bl_addr, iap->bl_size);
}

bool flash_iap_write_bl(const tFlashDriverOps *ops, FlashChipHandle h, const tFlashIAP *iap,
                        uint32_t offset, const uint8_t *data, uint32_t size)
{
    if (!iap || offset + size > iap->bl_size)
        return false;
    return flash_iap_write(ops, h, iap->bl_addr + offset, data, size);
}

bool flash_iap_verify_bl(const tFlashDriverOps *ops, FlashChipHandle h, const tFlashIAP *iap,
                         uint32_t offset, const uint8_t *data, uint32_t size)
{
    if (!iap || offset + size > iap->bl_size)
        return false;
    return flash_iap_verify(ops, h, iap->bl_addr + offset, data, size);
}

void flash_iap_jump_app(const tFlashIAP *iap)
{
    if (iap && iap->jump)
        iap->jump(iap->app_addr);
}

void flash_iap_jump_bl(const tFlashIAP *iap)
{
    if (iap && iap->jump)
        iap->jump(iap->bl_addr);
}

void flash_iap_reset(const tFlashIAP *iap)
{
    if (iap && iap->reset)
        iap->reset();
}
