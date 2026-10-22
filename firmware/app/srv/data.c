#include "data.h"

#include <string.h>

static const tDataField g_data_slots[DATA_NUM] = {
#define D_SLOT(id, field, type) \
    [id] = {&g_data.field, type},
    DATA_FIELDS(D_SLOT)
#undef D_SLOT
};

static const uint8_t TYPE_SIZE[] = {
    [FLD_U8] = sizeof(uint8_t),
    [FLD_U32] = sizeof(uint32_t),
    [FLD_F32] = sizeof(float),
    [FLD_BOOL] = sizeof(bool),
};

// 读一个通道：解引用绑定地址；未绑定返回 0.0f
void dm_data_get(eObsData data, uint8_t *val, uint8_t *len)
{
    if ((uint16_t)data >= DATA_NUM)
        return 0.0f;

    uint8_t n = TYPE_SIZE[g_data_slots[data].type];
    memcpy(val, g_data_slots[data].ptr, n);
    if (len)
        *len = n;
}
