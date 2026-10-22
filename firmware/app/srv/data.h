#ifndef __DATA_H
#define __DATA_H

#include "data_list.h"

#include <stdint.h>
#include <stdbool.h>

// 数据源绑定表：各成员指向实时数据地址，由组装层绑定（只读）
extern const tObsData g_data;

typedef struct
{
    void *ptr;    // 字段地址（&结构体.字段）
    uint8_t type; // eFieldType
} tDataField;

// 读一个通道（未绑定时返回 0.0f）
void dm_data_get(eObsData data, uint8_t *val, uint8_t *len);

#endif
