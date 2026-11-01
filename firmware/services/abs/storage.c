#include "storage.h"
#include "crc.h"
#include <string.h>

bool storage_init(tStorage *s, const tStorageDevHandle *handle) {
    if (!s || !handle || !handle->ops || !handle->dev)
        return false;

    memset(s, 0, sizeof(*s));
    s->handle = handle;

    if (!s->handle->ops->open(s->handle->dev, &s->size)) {
        s->dstate = DEV_RUN_ERROR;
        return false;
    }
    // 找空内存头
    uint8_t flag[2] = {0};
    uint32_t addr_head = 0;
    uint32_t addr_tail = s->size - 2;

    while (s->free_addr < s->size) {
        s->free_addr = (addr_tail + addr_head) / 2;
        s->handle->ops->read(s->handle->dev, s->free_addr, flag, 2);
        if (flag[0] == 0xFF && flag[1] == 0xFF) {
            addr_tail = s->free_addr;
            if (s->free_addr == 0)
                break;
        } else if (flag[0] != 0xFF && flag[1] != 0xFF) {
            addr_head = s->free_addr;
        } else { // 找到空内存头 返回
            break;
        }
    }
    s->dstate = DEV_ONLINE;
    return true;
}

bool storage_append(tStorage *s, const uint8_t *data, uint32_t len) {
    if (!s || !data)
        return false;

    if (s->free_addr + len > s->size) {
        if (!s->handle->ops->erase_addr(s->handle->dev, 0, s->size))
            return false;
        s->free_addr = 0;
        if (!s->handle->ops->write(s->handle->dev, s->free_addr, data, len))
            return false;
        s->free_addr += len;
    } else {
        if (!s->handle->ops->write(s->handle->dev, s->free_addr, data, len))
            return false;
        s->free_addr += len;
    }
    return true;
}

bool storage_read(tStorage *s, uint8_t *data, uint32_t len) {
    if (!s || !data)
        return false;
    if (s->free_addr < len)
        return false; // 空
    return s->handle->ops->read(s->handle->dev, s->free_addr - len, data, len);
}

bool storage_erase(tStorage *s) {
    if (!s)
        return false;
    if (!s->handle->ops->erase_addr(s->handle->dev, 0, s->size))
        return false;
    s->free_addr = 0;
    return true;
}
