#include "storage.h"
#include "parameters.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define PAGE 4096U
static uint8_t flash[4U * PAGE];
static int fail_write = -1;
static int write_calls;
static int fail_read_addr = -1;
static unsigned erased_pages[4];
tStorage g_storage;
tStorageUnit g_param_storage_unit;
tStorageUnit g_log_storage_unit;

static bool open_dev(void *d) { return d != NULL; }
static bool read_dev(void *d, uint32_t a, uint8_t *p, uint32_t n) {
    (void)d;
    if (fail_read_addr >= 0 && a <= (uint32_t)fail_read_addr &&
        n > (uint32_t)fail_read_addr - a) return false;
    if (a > sizeof(flash) || n > sizeof(flash) - a) return false;
    memcpy(p, flash + a, n);
    return true;
}
static bool write_dev(void *d, uint32_t a, const uint8_t *p, uint32_t n) {
    (void)d;
    assert((a & 7U) == 0 && (n & 7U) == 0);
    assert(a <= sizeof(flash) && n <= sizeof(flash) - a);
    if (write_calls++ == fail_write) {
        /* 模拟 double-word 内的撕裂写。 */
        for (uint32_t i = 0; i < n / 2U; ++i) flash[a + i] &= p[i];
        return false;
    }
    for (uint32_t i = 0; i < n; ++i) {
        assert(flash[a + i] == 0xFFU); /* 禁止重复编程，连 FF 也不重写。 */
        flash[a + i] = p[i];
    }
    return true;
}
static bool erase_dev(void *d, uint8_t page) {
    (void)d;
    assert(page < 4);
    ++erased_pages[page];
    memset(flash + page * PAGE, 0xFF, PAGE);
    return true;
}
static uint8_t count_dev(void *d) { (void)d; return 4; }
static uint32_t addr_dev(void *d, uint8_t p) { (void)d; return p * PAGE; }
static uint32_t size_dev(void *d, uint8_t p) { (void)d; (void)p; return PAGE; }
static const tStorageOps ops = {
    .open = open_dev, .read = read_dev, .write = write_dev,
    .erase_sector = erase_dev, .sector_count = count_dev,
    .sector_addr = addr_dev, .sector_size = size_dev,
};
static const tStorageDevHandle handle = {.ops = &ops, .dev = flash};
static void reboot(void) {
    fail_write = -1;
    write_calls = 0;
    assert(storage_init(&g_storage, &handle));
    assert(storage_unit_register(&g_storage, &g_param_storage_unit, 0, 3));
    assert(storage_unit_register(&g_storage, &g_log_storage_unit, 3, 1));
}
static void clean(void) {
    memset(flash, 0xFF, sizeof(flash));
    memset(erased_pages, 0, sizeof(erased_pages));
    reboot();
}
static void test_registration_and_payload(void) {
    clean();
    assert(g_storage.usr_sector_free == 0);
    tStorageUnit other;
    assert(!storage_unit_register(&g_storage, &other, 1, 1));
    assert(!storage_unit_register(&g_storage, &other, 3, 2));
    uint8_t data[101], result[101];
    memset(data, 0xFF, sizeof(data));
    data[50] = 0x12;
    assert(storage_unit_append(&g_param_storage_unit, data, sizeof(data)));
    reboot();
    assert(storage_unit_read(&g_param_storage_unit, result, sizeof(result)));
    assert(memcmp(data, result, sizeof(data)) == 0);
    assert(!storage_unit_read(&g_param_storage_unit, result, sizeof(result) - 1));
    for (unsigned i = 0; i < 200; ++i) {
        data[20] = (uint8_t)i;
        assert(storage_unit_append(&g_param_storage_unit, data, sizeof(data)));
        reboot();
        assert(storage_unit_read(&g_param_storage_unit, result, sizeof(result)));
        assert(memcmp(data, result, sizeof(data)) == 0);
    }
    assert(erased_pages[0] && erased_pages[1] && erased_pages[2]);
    assert(erased_pages[3] == 0);
    uint8_t log[1000]; memset(log, 0xA5, sizeof(log));
    for (unsigned i = 0; i < 10; ++i)
        assert(storage_unit_append(&g_log_storage_unit, log, sizeof(log)));
    assert(erased_pages[3] != 0);
    reboot();
    assert(storage_unit_read(&g_param_storage_unit, result, sizeof(result)));
    assert(memcmp(data, result, sizeof(data)) == 0);
}
static void test_torn_write(void) {
    uint8_t data[1000], out[1000], snapshot[sizeof(flash)];
    memset(data, 0x34, sizeof(data));
    clean();
    assert(storage_unit_append(&g_param_storage_unit, data, sizeof(data)));
    memcpy(snapshot, flash, sizeof(flash));
    /* header + 125 payload writes + commit，逐点注入失败。 */
    for (int failure = 0; failure < 127; ++failure) {
        memcpy(flash, snapshot, sizeof(flash));
        reboot();
        data[50] = 0x56;
        write_calls = 0; fail_write = failure;
        assert(!storage_unit_append(&g_param_storage_unit, data, sizeof(data)));
        reboot();
        assert(storage_unit_read(&g_param_storage_unit, out, sizeof(out)));
        assert(out[50] == 0x34);
        assert(storage_unit_append(&g_param_storage_unit, data, sizeof(data)));
        reboot();
        assert(storage_unit_read(&g_param_storage_unit, out, sizeof(out)));
        assert(memcmp(data, out, sizeof(data)) == 0);
    }
    /* 填满活跃页，再在跨页轮换的各写点失败，旧页始终可恢复。 */
    clean();
    for (int i = 0; i < 3; ++i)
        assert(storage_unit_append(&g_param_storage_unit, data, sizeof(data)));
    memcpy(snapshot, flash, sizeof(flash));
    for (int failure = 0; failure < 127; ++failure) {
        memcpy(flash, snapshot, sizeof(flash));
        reboot();
        write_calls = 0; fail_write = failure;
        data[50] = 0x78;
        assert(!storage_unit_append(&g_param_storage_unit, data, sizeof(data)));
        reboot();
        assert(storage_unit_read(&g_param_storage_unit, out, sizeof(out)));
        assert(out[50] == 0x56);
    }
}
static void test_parameters(void) {
    clean();
    assert(param_init(&g_storage));
    uint32_t version, size;
    param_upgrade_get(&version, &size);
    assert(version == UINT32_MAX && size == 0);
    assert(param_save());
    reboot();
    assert(param_init(&g_storage));
    param_upgrade_get(&version, &size);
    assert(version == UINT32_MAX);
    assert(param_upgrade_set(0x010203U, 1024));
    assert(param_save());
    reboot();
    assert(param_init(&g_storage));
    param_upgrade_get(&version, &size);
    assert(version == 0x010203U && size == 1024);
    write_calls = 0; fail_write = 0;
    assert(!param_upgrade_clear());
    param_upgrade_get(&version, &size);
    assert(version == 0x010203U);
    reboot();
    assert(param_init(&g_storage));
    param_upgrade_get(&version, &size);
    assert(version == 0x010203U && size == 1024);
    assert(param_upgrade_clear());
    reboot();
    assert(param_init(&g_storage));
    param_upgrade_get(&version, &size);
    assert(version == UINT32_MAX && size == 0);
    /* 旧/损坏数据不静默按空区处理。 */
    memset(flash, 0xFF, sizeof(flash)); flash[0] = 0;
    assert(storage_init(&g_storage, &handle));
    assert(!storage_unit_register(&g_storage, &g_param_storage_unit, 0, 3));
}
static void test_corruption_and_io(void) {
    uint8_t data[32], out[32];
    clean();
    memset(data, 0x11, sizeof(data));
    assert(storage_unit_append(&g_param_storage_unit, data, sizeof(data)));
    memset(data, 0x22, sizeof(data));
    assert(storage_unit_append(&g_param_storage_unit, data, sizeof(data)));
    flash[24] ^= 1U; /* 较早记录坏，不应遮蔽后续完整记录。 */
    reboot();
    assert(storage_unit_read(&g_param_storage_unit, out, sizeof(out)));
    assert(memcmp(data, out, sizeof(data)) == 0);
    fail_read_addr = (int)g_param_storage_unit.latest_addr;
    assert(storage_init(&g_storage, &handle));
    assert(!storage_unit_register(&g_storage, &g_param_storage_unit, 0, 3));
    fail_read_addr = -1;
    reboot();
    /* 日志首条半写：重启可注册，下一次追加会安全重建。 */
    fail_write = 0; write_calls = 0;
    assert(!storage_unit_append(&g_log_storage_unit, data, sizeof(data)));
    reboot();
    assert(storage_unit_append(&g_log_storage_unit, data, sizeof(data)));
    assert(storage_unit_read(&g_param_storage_unit, out, sizeof(out)));
    assert(memcmp(data, out, sizeof(data)) == 0);
}
int main(void) {
    test_corruption_and_io();
    test_registration_and_payload();
    test_torn_write();
    test_parameters();
    puts("storage tests passed");
}
