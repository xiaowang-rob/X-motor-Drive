
#include "iap.h"

void iap_init(tIAP *iap, tIAPDriverOps *ops, eIAPtype type)
{
    if (!iap || !ops)
        return;
    iap->ops = ops;
    iap->type = type;
    if (type == IAP_APP)
        iap->ops->app_init(); // 完成向量表偏移
}
// 便捷：擦写/校验整个 App / BL 区
bool iap_erase(tIAP *iap)
{
    if (!iap)
        return false;
    if (IAP_APP == iap->type)
        return iap->ops->app_erase();
    else
        return iap->ops->bl_erase();
}
bool iap_write(tIAP *iap, uint32_t offset,
               const uint8_t *data, uint32_t size)
{
    if (!iap || !data || size == 0U)
        return false;
    if (IAP_APP == iap->type)
        return iap->ops->app_write(offset, data, size);
    else
        return iap->ops->bl_write(offset, data, size);
}
bool iap_verify(tIAP *iap, uint32_t offset,
                const uint8_t *data, uint32_t size)
{
    if (!iap || !data || size == 0U)
        return false;
    uint8_t verify_data;
    if (IAP_APP == iap->type)
    {
        for (uint32_t i = 0; i < size; i++)
        {
            if (!iap->ops->app_read(offset + i, &verify_data, 1))
                return false;
            if (verify_data != data[i])
                return false;
        }
    }
    else
    {
        for (uint32_t i = 0; i < size; i++)
        {
            if (!iap->ops->bl_read(offset + i, &verify_data, 1))
                return false;
            if (verify_data != data[i])
                return false;
        }
    }
    return true;
}

// 跳转 / 复位
void iap_jump_app(tIAP *iap)
{
    if (!iap)
        return iap->ops->jump_app();
}
void iap_reset(tIAP *iap)
{
    if (!iap)
        return iap->ops->jump_bl();
}
