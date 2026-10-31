#include "bl_iap.h"

#include <string.h>
#include "bl_main.h"
#include "bl_status.h"
#include "cmd_list.h"
#include "crc.h"
#include "devices.h"
#include "parameters.h"
static const tStorageDevHandle *s_storage;
static const tBlAppPartition *s_app;
static uint32_t s_size;
static uint32_t s_received;
static uint8_t s_tail[8];
static uint8_t s_tail_len;
static bool s_erased;
static bool s_verified;
static bool s_boot_requested;

static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void write_u32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i)
        p[i] = (uint8_t)(v >> (8U * i));
}

bool bl_iap_init(const tStorageDevHandle *storage, const tBlAppPartition *app) {
    if (!storage || !storage->ops || !storage->ops->read ||
        !storage->ops->write || !storage->ops->erase_addr || !app ||
        !app->is_valid || !app->jump || !app->size ||
        (app->base & 7U) || (app->size & 7U) ||
        app->base > UINT32_MAX - app->size)
        return false;
    s_storage = storage;
    s_app = app;
    return true;
}

void bl_iap_start(void) {
    s_size = 0;
    s_received = 0;
    s_tail_len = 0;
    s_erased = false;
    s_verified = false;
    s_boot_requested = false;
    uint32_t version, size;
    param_upgrade_get(&version, &size);
    /* 任意非擦除标志都阻止自动启动，包括损坏/越界的请求。 */
    if (version == PARAM_NO_FIRMWARE && s_app && s_app->is_valid())
        s_boot_requested = true;
    bl_status_set(BL_STATUS_IDLE);
}

bool bl_iap_should_boot(void) { return s_boot_requested; }

bool bl_iap_boot(void) {
    s_boot_requested = false;
    if (!s_app || !s_app->is_valid()) {
        bl_status_set(BL_STATUS_ERROR);
        return false;
    }
    return s_app->jump();
}

bool bl_iap_erase(uint32_t size) {
    if (!s_app || !s_storage || size < 8U || size > s_app->size)
        return false;
    uint32_t version, requested_size;
    param_upgrade_get(&version, &requested_size);
    if (version == PARAM_NO_FIRMWARE || version > 0xFFFFFFU || requested_size != size)
        return false;
    s_erased = false;
    s_verified = false;
    s_boot_requested = false;
    s_received = 0;
    s_tail_len = 0;
    if (!s_storage->ops->erase_addr(s_storage->dev, s_app->base, s_app->size))
        return false;
    s_size = size;
    s_erased = true;
    return true;
}

bool bl_iap_write(uint32_t offset, const uint8_t *data, uint32_t len) {
    if (!s_erased || s_verified || !data || !len || offset != s_received ||
        offset > s_size || len > s_size - offset)
        return false;
    /* UART 的 124-byte 数据块并非 double-word 对齐。缓存尾部，
     * 每个 Flash double-word 只编程一次，最后一块以 FF 补齐。 */
    while (len) {
        uint32_t n = 8U - s_tail_len;
        if (n > len)
            n = len;
        memcpy(s_tail + s_tail_len, data, n);
        s_tail_len += n;
        s_received += n;
        data += n;
        len -= n;
        if (s_tail_len == 8U || s_received == s_size) {
            uint32_t addr = s_app->base + s_received - s_tail_len;
            memset(s_tail + s_tail_len, 0xFF, 8U - s_tail_len);
            if (!s_storage->ops->write(s_storage->dev, addr, s_tail, 8U)) {
                s_erased = false; /* 部分写失败后必须重新擦除，禁止继续确认。 */
                return false;
            }
            s_tail_len = 0;
        }
    }
    return true;
}

bool bl_iap_verify(uint32_t expected, uint32_t size) {
    if (!s_erased || size != s_size || s_received != size || s_tail_len)
        return false;
    uint8_t buf[128];
    uint32_t crc = UINT32_MAX;
    for (uint32_t offset = 0; offset < size;) {
        uint32_t n = size - offset;
        if (n > sizeof(buf))
            n = sizeof(buf);
        if (!s_storage->ops->read(s_storage->dev, s_app->base + offset, buf, n))
            return false;
        crc = crc32_update(crc, buf, n);
        offset += n;
    }
    s_verified = false;
    if ((crc ^ UINT32_MAX) != expected || !s_app->is_valid())
        return false;
    /* 先提交持久化状态，成功后才允许 EXIT；失败保留升级请求。 */
    if (!param_upgrade_clear())
        return false;
    s_verified = true;
    return true;
}

bool bl_iap_command(uint8_t cmd, const uint8_t *data, uint16_t len,
                    uint8_t *response, uint16_t *response_len) {
    if (!response || !response_len || (len && !data))
        return false;
    bool ok = false;
    *response_len = 1;
    switch (cmd) {
    case CMD_CONNECT:
        ok = len == 1 && data[0] <= 1U;
        break;
    case CMD_ASK_BL_VERSION:
        if (len != 0)
            break;
        /* 当前版本查询响应是 UTF-8 文本，不是 U32。 */
        static const char version[] = "1.0.0";
        memcpy(response, version, sizeof(version) - 1U);
        *response_len = sizeof(version) - 1U;
        return true;
    case CMD_IAP_ERASE_FLASH:
        if (len == 4)
            ok = bl_iap_erase(read_u32(data));
        break;
    case CMD_IAP_WRITE_FLASH: {
        uint32_t offset = len >= 4 ? read_u32(data) : UINT32_MAX;
        if (len > 4 && len <= 128)
            ok = bl_iap_write(offset, data + 4, len - 4);
        write_u32(response, offset);
        response[4] = ok ? FB_EXECUTE : FB_FAILURE;
        *response_len = 5;
        bl_status_set(ok ? BL_STATUS_WRITING : BL_STATUS_ERROR);
        return ok;
    }
    case CMD_IAP_VERIFY_FLASH:
        if (len == 8)
            ok = bl_iap_verify(read_u32(data), read_u32(data + 4));
        break;
    case CMD_IAP_EXIT:
        ok = len == 0 && s_verified;
        s_boot_requested = ok;
        *response_len = 0; /* 当前协议 EXIT 无应答。 */
        bl_status_set(ok ? BL_STATUS_SUCCESS : BL_STATUS_ERROR);
        return ok;
    default:
        break;
    }
    response[0] = ok ? FB_EXECUTE : FB_FAILURE;
    bl_status_set(!ok ? BL_STATUS_ERROR :
                  (cmd == CMD_IAP_VERIFY_FLASH ? BL_STATUS_SUCCESS :
                   (cmd == CMD_IAP_ERASE_FLASH ? BL_STATUS_WRITING : BL_STATUS_IDLE)));
    return ok;
}
