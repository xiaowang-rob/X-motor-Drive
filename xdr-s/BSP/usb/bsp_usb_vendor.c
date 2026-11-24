/* XDr framing over the TinyUSB Vendor byte stream. */
#include "bsp_usb_vendor.h"

#include "bsp_usb.h"
#include "stm32g4xx_hal.h"
#include "tusb.h"

#define USB_RING_SIZE 8192U
#define USB_RING_MASK (USB_RING_SIZE - 1U)
#define USB_WIRE_MAX (BSP_USB_VENDOR_MAX_PAYLOAD - 1U + BSP_USB_VENDOR_OVERHEAD)

_Static_assert(CFG_TUD_VENDOR_TX_BUFSIZE >= USB_WIRE_MAX,
               "Vendor TX FIFO must fit a complete XDr frame");

static struct {
    usb_rx_frame_cb rx_cb;
    void *rx_ctx;
} usb_vendor;
static tUsbVendorStats stats;

static uint8_t rx_ring[USB_RING_SIZE];
static uint32_t rx_head, rx_tail, rx_last_ms;
static uint8_t rx_payload[BSP_USB_VENDOR_MAX_PAYLOAD];
static uint8_t tx_frame[USB_WIRE_MAX];

static uint16_t crc16_byte(uint16_t crc, uint8_t byte) {
    crc ^= (uint16_t)byte << 8;
    for (uint8_t bit = 0U; bit < 8U; ++bit)
        crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U)
                              : (uint16_t)(crc << 1);
    return crc;
}

static uint8_t rx_at(uint32_t offset) {
    return rx_ring[(rx_tail + offset) & USB_RING_MASK];
}

static bool bsp_usb_vendor_ready(void) {
    return tud_ready() && tud_vendor_mounted();
}

static bool bsp_usb_vendor_send(void *dev, uint8_t id, const uint8_t *data,
                                uint16_t len) {
    if (dev != &usb_vendor || __get_IPSR() != 0U)
        return false;

    uint16_t wire_len = (uint16_t)(len + BSP_USB_VENDOR_OVERHEAD);
    if ((len != 0U && data == NULL) || len >= BSP_USB_VENDOR_MAX_PAYLOAD ||
        !bsp_usb_vendor_ready() || tud_vendor_write_available() < wire_len) {
        ++stats.tx_rejected;
        return false;
    }

    tx_frame[0] = 0xA5U;
    tx_frame[1] = 0x5AU;
    tx_frame[2] = id;
    tx_frame[3] = (uint8_t)len;
    tx_frame[4] = (uint8_t)(len >> 8);
    if (len != 0U)
        memcpy(tx_frame + BSP_USB_VENDOR_HEADER_SIZE, data, len);

    uint16_t crc = 0xFFFFU;
    for (uint16_t i = 0U; i < wire_len - 2U; ++i)
        crc = crc16_byte(crc, tx_frame[i]);
    tx_frame[wire_len - 2U] = (uint8_t)crc;
    tx_frame[wire_len - 1U] = (uint8_t)(crc >> 8);

    /* 写入、回调和 USB 事件处理都在同一主循环上下文。 */
    if (tud_vendor_write(tx_frame, wire_len) != wire_len) {
        ++stats.tx_submit_errors;
        return false;
    }
    tud_vendor_write_flush();
    ++stats.tx_messages;
    return true;
}

static void rx_parse(void) {
    while (rx_head - rx_tail >= 2U) {
        if (rx_at(0U) != 0xA5U || rx_at(1U) != 0x5AU) {
            ++rx_tail;
            continue;
        }
        if (rx_head - rx_tail < BSP_USB_VENDOR_HEADER_SIZE)
            return;
        uint16_t len = (uint16_t)rx_at(3U) | ((uint16_t)rx_at(4U) << 8);
        if (len >= BSP_USB_VENDOR_MAX_PAYLOAD) {
            ++stats.rx_header_errors;
            ++rx_tail;
            continue;
        }
        uint32_t total = (uint32_t)len + BSP_USB_VENDOR_OVERHEAD;
        if (rx_head - rx_tail < total)
            return;
        uint16_t crc = 0xFFFFU;
        for (uint32_t i = 0U; i < total - 2U; ++i)
            crc = crc16_byte(crc, rx_at(i));
        uint16_t received =
            (uint16_t)rx_at(total - 2U) | ((uint16_t)rx_at(total - 1U) << 8);
        if (crc != received) {
            ++stats.rx_crc_errors;
            ++rx_tail; /* Rescan buffered bytes; a valid magic may follow. */
            continue;
        }
        rx_payload[0] = rx_at(2U);
        for (uint16_t i = 0U; i < len; ++i)
            rx_payload[i + 1U] = rx_at(BSP_USB_VENDOR_HEADER_SIZE + i);
        rx_tail += total;
        ++stats.rx_messages;
        if (usb_vendor.rx_cb != NULL)
            usb_vendor.rx_cb(
                usb_vendor.rx_ctx, rx_payload, (uint16_t)(len + 1U));
        else
            ++stats.rx_no_callback;
    }
}

static void receive_bytes(const uint8_t *data, uint16_t len) {
    if (data == NULL || len == 0U)
        return; /* A short packet/ZLP is not an application message boundary. */
    if (len > USB_RING_SIZE - (rx_head - rx_tail)) {
        ++stats.rx_overflows;
        rx_tail = rx_head;
        if (len > USB_RING_SIZE)
            return;
    }
    for (uint16_t i = 0U; i < len; ++i)
        rx_ring[rx_head++ & USB_RING_MASK] = data[i];
    rx_parse();
}

static void rx_check_timeout(uint32_t now) {
    if (rx_head == rx_tail ||
        (uint32_t)(now - rx_last_ms) < BSP_USB_VENDOR_RX_TIMEOUT_MS)
        return;

    ++stats.rx_timeouts;
    /* 损坏的长度字段可能遮住后续合法帧。 */
    ++rx_tail;
    rx_parse();
    rx_tail = rx_head;
}

void tud_vendor_rx_cb(uint8_t index, const uint8_t *buffer, uint32_t size) {
    (void)buffer;
    (void)size;
    if (index != 0U)
        return;

    uint8_t packet[CFG_TUD_VENDOR_RX_EPSIZE];
    uint32_t now = HAL_GetTick();
    rx_check_timeout(now);
    uint32_t count;
    while ((count = tud_vendor_read(packet, sizeof(packet))) != 0U) {
        rx_last_ms = now;
        receive_bytes(packet, (uint16_t)count);
    }
}

void bsp_usb_vendor_reset(void) {
    rx_head = rx_tail = 0U;
}

void bsp_usb_vendor_get_stats(tUsbVendorStats *out) {
    if (out != NULL)
        *out = stats;
}

static bool bsp_usb_vendor_open(void *dev) {
    return dev == &usb_vendor && bsp_usb_start(false);
}

static void bsp_usb_vendor_set_rx_cb(void *dev, usb_rx_frame_cb cb, void *ctx) {
    if (dev == &usb_vendor) {
        usb_vendor.rx_ctx = ctx;
        usb_vendor.rx_cb = cb;
    }
}

static bool bsp_usb_vendor_is_ready(void *dev) {
    return dev == &usb_vendor && bsp_usb_vendor_ready();
}

static void bsp_usb_vendor_task(void *dev) {
    if (dev == &usb_vendor) {
        bsp_usb_task();
        if (tud_mounted())
            rx_check_timeout(HAL_GetTick());
        else
            bsp_usb_vendor_reset();
    }
}

const tUsbOps usb_vendor_ops = {
    .open = bsp_usb_vendor_open,
    .send = bsp_usb_vendor_send,
    .set_rx_cb = bsp_usb_vendor_set_rx_cb,
    .is_ready = bsp_usb_vendor_is_ready,
    .poll = bsp_usb_vendor_task,
};

static const tUsbDevHandle usb_vendor_handle = {
    .ops = &usb_vendor_ops,
    .dev = &usb_vendor,
};

const tUsbDevHandle *Get_USB_VendorHandle(void) { return &usb_vendor_handle; }
