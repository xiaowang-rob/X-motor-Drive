#ifndef XDR_STORAGE_H
#define XDR_STORAGE_H
#include "device_status.h"

/* 地址为绝对地址。write 必须支持 8-byte 对齐编程，erase_sector 只擦用户页。 */
typedef struct {
    bool (*open)(void *dev, uint32_t *size);
    bool (*read)(void *dev, uint32_t addr, uint8_t *data, uint32_t len);
    bool (*write)(void *dev, uint32_t addr, const uint8_t *data, uint32_t len);
    bool (*erase_addr)(void *dev, uint32_t addr, uint32_t len);
} tStorageOps;
typedef struct {
    const tStorageOps *ops;
    void *dev;
} tStorageDevHandle;
typedef struct {
    const tStorageDevHandle *handle;
    eDeviceStatus dstate;
    uint32_t size;
    uint32_t free_addr;   /* 活跃页内偏移 */
    uint32_t free_offset; /* 活跃页内偏移  因为double
                             word对齐写入，所以需要记录偏移量 */
} tStorage;
extern tStorage g_param_storage;
extern tStorage g_log_storage;

bool storage_init(tStorage *s, const tStorageDevHandle *handle);

bool storage_append(tStorage *s, const uint8_t *data, uint32_t len);
/* 读取最新提交记录，len 必须与记录长度一致；无记录时返回 false。 */
bool storage_read(tStorage *s, uint8_t *data, uint32_t len);
/* 显式清空所有页，不提供掉电原子性；参数正常保存不调用此接口。 */
bool storage_erase(tStorage *s);
#endif
