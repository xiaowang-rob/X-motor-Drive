#include "obsdata.h"

#include <string.h>

void dm_data_init(tObsDataService *dm, const tObsData *data) {
#define D_SLOT(id, field, type)                                                \
    dm->slots[id] = (tDataField){data->field, type};
    DATA_FIELDS(D_SLOT)

#undef D_SLOT
}

// 读一个通道：解引用绑定地址；未绑定返回 0.0f
void dm_data_get(const tObsDataService *dm, eObsData data, uint8_t *val, uint8_t *len) {
    if ((uint16_t) data >= DATA_NUM || !len) {
        *val = 0.0f;
        *len = 0;
        return;
    }

    *len = field_value_type_size(dm->slots[data].type);
    memcpy(val, dm->slots[data].ptr, *len);
}
