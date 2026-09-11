#ifndef __IAP_H
#define __IAP_H

#include "device.h"

typedef struct
{
    void (*init)(void);
    uint32_t (*get_bl_ids)(uint8_t *num);
    uint32_t (*get_app_ids)(uint8_t *num); // 获取 App 分区 ID
    uint32_t (*get_usr_ids)(uint8_t *num); // 获取用户分区 ID
    uint32_t (*get_sector_addr)(uint8_t sec_id);
    uint32_t (*get_sector_size)(uint8_t sec_id);
    bool (*jump_app)(void);
    bool (*jump_bl)(void);
} tIAPDriverOps;

#endif