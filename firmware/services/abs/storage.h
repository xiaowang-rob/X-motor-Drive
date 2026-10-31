#ifndef XDR_STORAGE_H
#define XDR_STORAGE_H
#include "device_status.h"

/* 地址为绝对地址。write 必须支持 8-byte 对齐编程，erase_sector 只擦用户页。 */
typedef struct {
    bool (*open)(void *handle);
    bool (*read)(void *handle, uint32_t addr, uint8_t *data, uint32_t len);
    bool (*write)(void *handle, uint32_t addr, const uint8_t *data, uint32_t len);
    bool (*erase_addr)(void *handle, uint32_t addr, uint32_t len);
    bool (*erase_sector)(void *handle, uint8_t sec_id);
    uint8_t (*sector_count)(void *handle);
    uint32_t (*sector_addr)(void *handle, uint8_t sec_id);
    uint32_t (*sector_size)(void *handle, uint8_t sec_id);
} tStorageOps;
typedef struct {
    const tStorageOps *ops;
    void *dev;
} tStorageDevHandle;
typedef struct {
    const tStorageDevHandle *handle;
    eDeviceStatus dstate;
    uint16_t usr_sector_bit_status;
    uint32_t usr_sector_count;
    uint8_t usr_sector_free;
} tStorage;
extern tStorage g_storage;

/* 连续若干擦除页构成一个 unit。抽象层拥有记录封装、CRC、提交和轮换。
 * 多页 unit 先在非最新页提交新记录，之后才允许回收旧页。
 * 单页 unit（日志）写满会擦除该页，不承诺掉电时保留旧日志。
 * 记录格式为本层私有；不支持按任意 payload 的 FF 分布恢复旧格式。
 * unit 仅允许主循环串行调用，无堆分配。 */
typedef struct {
    tStorage *storage;
    uint8_t first_sector;
    uint8_t sector_count;
    uint8_t active_sector;
    uint32_t free_addr; /* 活跃页内偏移 */
    uint32_t latest_addr;
    uint32_t latest_size;
    uint32_t sequence;
    bool has_record;
    bool writable;
} tStorageUnit;
extern tStorageUnit g_param_storage_unit;
extern tStorageUnit g_log_storage_unit;

bool storage_init(tStorage *s, const tStorageDevHandle *handle);
bool storage_unit_register(tStorage *s, tStorageUnit *unit,
                           uint8_t first_sector, uint8_t sector_count);
void storage_unit_unregister(tStorageUnit *unit);
bool storage_unit_append(tStorageUnit *unit, const uint8_t *data, uint32_t len);
/* 读取最新提交记录，len 必须与记录长度一致；无记录时返回 false。 */
bool storage_unit_read(tStorageUnit *unit, uint8_t *data, uint32_t len);
/* 显式清空所有页，不提供掉电原子性；参数正常保存不调用此接口。 */
bool storage_unit_erase(tStorageUnit *unit);
#endif
