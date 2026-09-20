// ============================================================
// iap.c — 在线升级业务对象（abs，纯逻辑）
//
// 只做"分区转发 + 状态维护"：所有 Flash 操作走复用的介质
// （tFlashOps + handle），跳转走板级注入的回调；整区校验用 CRC32 流式计算。
// ============================================================

#include "iap.h"

#include <string.h>

#include "crc.h"

#define IAP_CRC_CHUNK 256U // 流式校验分块大小（栈占用）

// 取分区几何（type 越界或分区大小为 0 时返回 NULL）
static const tIAPPartition *part_of(const tIAP *iap, eIAPtype type)
{
    if (!iap || !iap->cfg.parts)
        return NULL;
    if ((uint8_t)type >= iap->cfg.part_count)
        return NULL;

    const tIAPPartition *p = &iap->cfg.parts[type];
    return (p->size == 0U) ? NULL : p;
}

// 介质读（句柄取自装配结果）
static inline bool iap_rd(const tIAP *iap, uint32_t addr, uint8_t *data, uint32_t len)
{
    return iap->cfg.flash_ops->read(iap->cfg.flash_handle, addr, data, len);
}

bool iap_init(tIAP *iap)
{
    if (!iap || !iap->cfg.parts || !iap->cfg.flash_ops)
        return false;
    if (!iap->cfg.flash_ops->read || !iap->cfg.flash_ops->write ||
        !iap->cfg.flash_ops->erase_addr)
        return false;
    if (iap->cfg.part_count == 0U)
        return false;

    iap->dstate = DEV_ONLINE;
    return true;
}

const tIAPPartition *iap_get_partition(const tIAP *iap, eIAPtype type)
{
    return part_of(iap, type);
}

bool iap_erase(tIAP *iap)
{
    if (!iap)
        return false;
    const tIAPPartition *p = part_of(iap, iap->cfg.type);
    if (!p)
        return false;

    if (!iap->cfg.flash_ops->erase_addr(iap->cfg.flash_handle, p->base, p->size))
    {
        iap->dstate = DEV_RUN_ERROR;
        return false;
    }
    iap->dstate = DEV_RUNNING;
    return true;
}

bool iap_write(tIAP *iap, uint32_t offset, const uint8_t *data, uint32_t len)
{
    if (!iap || !data || len == 0U)
        return false;
    const tIAPPartition *p = part_of(iap, iap->cfg.type);
    if (!p || (offset + len) > p->size)
        return false;

    if (!iap->cfg.flash_ops->write(iap->cfg.flash_handle, p->base + offset, data, len))
    {
        iap->dstate = DEV_RUN_ERROR;
        return false;
    }
    iap->dstate = DEV_RUNNING;
    return true;
}

bool iap_read(tIAP *iap, uint32_t offset, uint8_t *data, uint32_t len)
{
    if (!iap || !data || len == 0U)
        return false;
    const tIAPPartition *p = part_of(iap, iap->cfg.type);
    if (!p || (offset + len) > p->size)
        return false;

    return iap_rd(iap, p->base + offset, data, len);
}

bool iap_verify_crc(tIAP *iap, uint32_t expect_crc)
{
    if (!iap)
        return false;
    const tIAPPartition *p = part_of(iap, iap->cfg.type);
    if (!p)
        return false;

    uint8_t buf[IAP_CRC_CHUNK];
    uint32_t crc = 0xFFFFFFFFU; // 标准 CRC32 初值
    uint32_t done = 0U;

    while (done < p->size)
    {
        uint32_t n = p->size - done;
        if (n > IAP_CRC_CHUNK)
            n = IAP_CRC_CHUNK;

        if (!iap_rd(iap, p->base + done, buf, n))
        {
            iap->dstate = DEV_RUN_ERROR;
            return false;
        }
        crc = crc32_update(crc, buf, n);
        done += n;
    }

    crc ^= 0xFFFFFFFFU;
    bool ok = (crc == expect_crc);
    iap->dstate = ok ? DEV_RUNNING : DEV_RUN_ERROR;
    return ok;
}

bool iap_jump(tIAP *iap)
{
    if (!iap || !iap->cfg.jump)
        return false;
    return iap->cfg.jump(iap->cfg.type);
}

bool iap_reset(tIAP *iap)
{
    if (!iap || !iap->cfg.jump)
        return false;
    return iap->cfg.jump(IAP_BL); // 复位回 bootloader
}
