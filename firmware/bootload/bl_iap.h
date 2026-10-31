#ifndef XDR_BL_IAP_H
#define XDR_BL_IAP_H

#include "storage.h"

/* 板级注入 APP 分区与跳转能力，业务层不依赖 HAL。 */
typedef struct {
    uint32_t base;
    uint32_t size;
    bool (*is_valid)(void);
    bool (*jump)(void);
} tBlAppPartition;

bool bl_iap_init(const tStorageDevHandle *storage, const tBlAppPartition *app);
void bl_iap_start(void);
bool bl_iap_should_boot(void);
bool bl_iap_boot(void);
bool bl_iap_erase(uint32_t size);
bool bl_iap_write(uint32_t offset, const uint8_t *data, uint32_t len);
bool bl_iap_verify(uint32_t crc, uint32_t size);
/* 无外层帧的业务命令处理；response 为 payload，不含 command id。 */
bool bl_iap_command(uint8_t cmd, const uint8_t *data, uint16_t len,
                    uint8_t *response, uint16_t *response_len);

#endif
