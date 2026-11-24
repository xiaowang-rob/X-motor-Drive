#ifndef __OBSDATA_H
#define __OBSDATA_H

#include "data_list.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    void *ptr;    // 字段地址（&结构体.字段）
    uint8_t type; // eFieldType
} tDataField;

typedef struct {
    tDataField slots[DATA_NUM];
} tObsDataService;

void dm_data_init(tObsDataService *dm, const tObsData *data);
// 读一个通道（未绑定时返回 0.0f）
void dm_data_get(const tObsDataService *dm, eObsData data, uint8_t *val, uint8_t *len);

#endif
