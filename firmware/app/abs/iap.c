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

bool iap_erase(tIAP *iap)
{
    if (!iap)
        return false;

    return iap->flash_ops->erase_addr(iap->flash_handle, iap->parts->base, iap->parts->size);
}

bool iap_write(tIAP *iap, uint32_t offset, const uint8_t *data, uint32_t len)
{
    if (!iap || !data || len == 0U)
        return false;

    return iap->flash_ops->write(iap->flash_handle, iap->parts->base + offset, data, len);
}

bool iap_read(tIAP *iap, uint32_t offset, uint8_t *data, uint32_t len)
{
    if (!iap || !data || len == 0U)
        return false;

    return iap->flash_ops->read(iap->flash_handle, iap->parts->base + offset, data, len);
}

bool iap_verify_crc(tIAP *iap, uint32_t expect_crc)
{
    if (!iap)
        return false;

    uint8_t buf[IAP_CRC_CHUNK];
    uint32_t crc = 0xFFFFFFFFU; // 标准 CRC32 初值
    uint32_t done = 0U;

    while (done < iap->parts->size)
    {
        uint32_t n = iap->parts->size - done;
        if (n > IAP_CRC_CHUNK)
            n = IAP_CRC_CHUNK;

        if (!iap->flash_ops->read(iap->flash_handle, iap->parts->base + done, buf, n))
        {
            return false;
        }
        crc = crc32_update(crc, buf, n);
        done += n;
    }

    crc ^= 0xFFFFFFFFU;
    bool ok = (crc == expect_crc);
    return ok;
}

bool iap_jump(tIAP *iap)
{
    if (!iap || !iap->jump)
        return false;
    return iap->jump();
}
