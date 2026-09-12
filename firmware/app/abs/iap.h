#ifndef __IAP_H
#define __IAP_H

#include "flash.h"

typedef enum
{
    IAP_BL,
    IAP_APP
} eIAPtype;
typedef struct
{
    void (*app_init)(void);
    bool (*app_erase)(void);
    bool (*app_write)(uint32_t offset, const uint8_t *data, uint32_t len);
    bool (*app_read)(uint32_t offset, uint8_t *data, uint32_t len);
    bool (*bl_erase)(void);
    bool (*bl_write)(uint32_t offset, const uint8_t *data, uint32_t len);
    bool (*bl_read)(uint32_t offset, uint8_t *data, uint32_t len);
    bool (*jump_app)(void);
    bool (*jump_bl)(void);
} tIAPDriverOps;

typedef struct
{
    tIAPDriverOps *ops;
    eIAPtype type;
} tIAP;

void iap_init(tIAP *iap, tIAPDriverOps *ops, eIAPtype type);
// 便捷：擦写/校验整个 App / BL 区
bool iap_erase(tIAP *iap);
bool iap_write(tIAP *iap, uint32_t offset,
               const uint8_t *data, uint32_t size);
bool iap_verify(tIAP *iap, uint32_t offset,
                const uint8_t *data, uint32_t size);

// 跳转 / 复位
void iap_jump_app(tIAP *iap);
void iap_reset(tIAP *iap);

#endif