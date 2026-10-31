#include "storage.h"
#include "crc.h"
#include <string.h>

#define RECORD_MAGIC 0x31525453U
#define COMMIT_MAGIC 0x54494D43U
#define HEADER_SIZE 24U
#define COMMIT_SIZE 8U

/* 每项都是 32-bit 小端字；不将带 padding 的 C struct 写入介质。 */
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i)
        p[i] = (uint8_t)(v >> (8U * i));
}
static bool erased(const uint8_t *p, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i)
        if (p[i] != 0xFFU)
            return false;
    return true;
}
static const tStorageOps *ops(const tStorageUnit *u) { return u->storage->handle->ops; }
static void *dev(const tStorageUnit *u) { return u->storage->handle->dev; }
static bool rd(const tStorageUnit *u, uint32_t a, uint8_t *p, uint32_t n) {
    return ops(u)->read(dev(u), a, p, n);
}
static uint32_t page_size(const tStorageUnit *u, uint8_t sector) {
    return ops(u)->sector_size(dev(u), sector);
}
static uint32_t page_base(const tStorageUnit *u, uint8_t sector) {
    return ops(u)->sector_addr(dev(u), sector);
}
static uint32_t record_size(uint32_t len) {
    return HEADER_SIZE + ((len + 7U) & ~7U) + COMMIT_SIZE;
}
static uint32_t payload_crc(uint32_t seq, const uint8_t *data, uint32_t len) {
    uint8_t meta[8];
    put32(meta, seq);
    put32(meta + 4, len);
    return crc32_update(crc32_update(UINT32_MAX, meta, 8), data, len) ^ UINT32_MAX;
}
/* -1：介质读取失败；0：校验不符；1：校验通过。 */
static int disk_crc(tStorageUnit *u, uint32_t addr, uint32_t len,
                    uint32_t seq, uint32_t expected) {
    uint8_t buf[64];
    put32(buf, seq);
    put32(buf + 4, len);
    uint32_t crc = crc32_update(UINT32_MAX, buf, 8);
    while (len) {
        uint32_t n = len < sizeof(buf) ? len : sizeof(buf);
        if (!rd(u, addr, buf, n))
            return -1;
        crc = crc32_update(crc, buf, n);
        addr += n;
        len -= n;
    }
    return (crc ^ UINT32_MAX) == expected;
}

bool storage_init(tStorage *s, const tStorageDevHandle *handle) {
    if (!s || !handle || !handle->ops || !handle->dev)
        return false;
    const tStorageOps *o = handle->ops;
    if (!o->open || !o->read || !o->write || !o->erase_sector ||
        !o->sector_count || !o->sector_addr || !o->sector_size)
        return false;
    memset(s, 0, sizeof(*s));
    s->handle = handle;
    s->usr_sector_count = o->sector_count(handle->dev);
    if (!s->usr_sector_count || s->usr_sector_count > 16U || !o->open(handle->dev)) {
        s->dstate = DEV_RUN_ERROR;
        return false;
    }
    s->usr_sector_free = (uint8_t)s->usr_sector_count;
    s->dstate = DEV_ONLINE;
    return true;
}

bool storage_unit_register(tStorage *s, tStorageUnit *u,
                           uint8_t first, uint8_t count) {
    if (!s || !s->handle || !u || !count || first >= s->usr_sector_count ||
        count > s->usr_sector_count - first)
        return false;
    uint16_t mask = (uint16_t)(((1UL << count) - 1UL) << first);
    if (s->usr_sector_bit_status & mask)
        return false;
    memset(u, 0, sizeof(*u));
    u->storage = s;
    u->first_sector = first;
    u->sector_count = count;
    u->active_sector = first;
    bool dirty = false;
    for (uint8_t i = 0; i < count; ++i) {
        uint8_t sector = first + i;
        uint32_t size = page_size(u, sector), base = page_base(u, sector);
        if (size < HEADER_SIZE + COMMIT_SIZE + 8U || (size & 7U) ||
            (base & 7U) || base > UINT32_MAX - size)
            return false;
        if (i && base != page_base(u, sector - 1U) + page_size(u, sector - 1U))
            return false;
        uint32_t off = 0;
        while (off <= size - HEADER_SIZE) {
            uint8_t h[HEADER_SIZE], commit[COMMIT_SIZE];
            if (!rd(u, base + off, h, sizeof(h)))
                return false;
            if (erased(h, sizeof(h)))
                break;
            dirty = true;
            uint32_t seq = get32(h + 4), len = get32(h + 8);
            if (get32(h) != RECORD_MAGIC || get32(h + 16) != ~len ||
                get32(h + 20) != ~seq || !len || len > size - HEADER_SIZE - COMMIT_SIZE ||
                record_size(len) > size - off) {
                off = size; /* 撕裂头/旧格式：封住本页尾部，不重写已编程双字。 */
                break;
            }
            uint32_t total = record_size(len);
            if (!rd(u, base + off + total - COMMIT_SIZE, commit, sizeof(commit)))
                return false;
            int valid = disk_crc(u, base + off + HEADER_SIZE, len, seq, get32(h + 12));
            if (valid < 0)
                return false;
            if (get32(commit) != COMMIT_MAGIC || get32(commit + 4) != ~seq || !valid) {
                /* 长度和反码可信，跳过坏记录，仍扫描后续已提交记录。 */
                off += total;
                continue;
            }
            if (!u->has_record || (int32_t)(seq - u->sequence) > 0) {
                u->has_record = true;
                u->sequence = seq;
                u->latest_addr = base + off + HEADER_SIZE;
                u->latest_size = len;
                u->active_sector = sector;
            }
            off += total;
        }
        if (u->active_sector == sector)
            u->free_addr = off;
    }
    /* 全新擦除区可用；非空却无有效提交记录必须显式报错，不能默认为无升级。 */
    if (dirty && !u->has_record) {
        if (count > 1U)
            return false;
        /* 单页日志允许丢弃撕裂记录，下一次追加先擦除重建。 */
        u->free_addr = page_size(u, first);
    }
    u->writable = true;
    s->usr_sector_bit_status |= mask;
    s->usr_sector_free -= count;
    return true;
}

void storage_unit_unregister(tStorageUnit *u) {
    if (!u || !u->storage || !u->writable)
        return;
    uint16_t mask = (uint16_t)(((1UL << u->sector_count) - 1UL) << u->first_sector);
    u->storage->usr_sector_bit_status &= (uint16_t)~mask;
    u->storage->usr_sector_free += u->sector_count;
    u->writable = false;
}

bool storage_unit_append(tStorageUnit *u, const uint8_t *data, uint32_t len) {
    if (!u || !u->storage || !u->writable || !data || !len || len > UINT32_MAX - 39U)
        return false;
    uint32_t total = record_size(len);
    /* 所有页都必须能容纳该记录，以保证轮换可持续。 */
    for (uint8_t i = 0; i < u->sector_count; ++i)
        if (total > page_size(u, u->first_sector + i))
            return false;
    uint8_t sector = u->active_sector;
    uint32_t off = u->free_addr;
    uint32_t size = page_size(u, sector);
    if (off > size || total > size - off) {
        sector = u->first_sector + (sector - u->first_sector + 1U) % u->sector_count;
        /* 多页时只擦非最新页；擦写失败时原记录仍然有效。 */
        if (!ops(u)->erase_sector(dev(u), sector))
            goto fail;
        off = 0;
    }
    uint32_t seq = u->sequence + 1U;
    uint8_t h[HEADER_SIZE], block[8];
    put32(h, RECORD_MAGIC);
    put32(h + 4, seq);
    put32(h + 8, len);
    put32(h + 12, payload_crc(seq, data, len));
    put32(h + 16, ~len);
    put32(h + 20, ~seq);
    uint32_t addr = page_base(u, sector) + off;
    if (!ops(u)->write(dev(u), addr, h, sizeof(h)))
        goto fail;
    for (uint32_t pos = 0; pos < len; pos += 8U) {
        uint32_t n = len - pos < 8U ? len - pos : 8U;
        memset(block, 0xFF, sizeof(block));
        memcpy(block, data + pos, n);
        if (!ops(u)->write(dev(u), addr + HEADER_SIZE + pos, block, sizeof(block)))
            goto fail;
    }
    put32(block, COMMIT_MAGIC);
    put32(block + 4, ~seq);
    if (!ops(u)->write(dev(u), addr + total - COMMIT_SIZE, block, sizeof(block)))
        goto fail;
    if (disk_crc(u, addr + HEADER_SIZE, len, seq, get32(h + 12)) != 1)
        goto fail;
    u->active_sector = sector;
    u->free_addr = off + total;
    u->latest_addr = addr + HEADER_SIZE;
    u->latest_size = len;
    u->sequence = seq;
    u->has_record = true;
    u->storage->dstate = DEV_RUNNING;
    return true;
fail:
    /* 本次可能已部分编程。下一次到非最新页重新擦除，不在失败位置续写。 */
    u->free_addr = page_size(u, u->active_sector);
    u->storage->dstate = DEV_RUN_ERROR;
    return false;
}

bool storage_unit_read(tStorageUnit *u, uint8_t *data, uint32_t len) {
    if (!u || !u->storage || !u->has_record || !data || len != u->latest_size)
        return false;
    return rd(u, u->latest_addr, data, len);
}

bool storage_unit_erase(tStorageUnit *u) {
    if (!u || !u->storage || !u->writable)
        return false;
    for (uint8_t i = 0; i < u->sector_count; ++i)
        if (!ops(u)->erase_sector(dev(u), u->first_sector + i)) {
            u->writable = false;
            return false;
        }
    u->has_record = false;
    u->latest_size = 0;
    u->sequence = 0;
    u->active_sector = u->first_sector;
    u->free_addr = 0;
    return true;
}
