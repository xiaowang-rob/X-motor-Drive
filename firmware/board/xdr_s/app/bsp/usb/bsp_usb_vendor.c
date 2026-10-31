/* Vendor Bulk transport; UART-compatible payload bridge. No heap allocation. */
#include "bsp_usb_vendor.h"
#include "xdr_usbd_vendor_if.h"
#include <string.h>

#define USB_RING_SIZE 8192U
#define USB_RING_MASK (USB_RING_SIZE - 1U)

typedef struct tUsbVendor {
    uart_rx_done_cb rx_cb;
    void *rx_ctx;
} tUsbVendor;

tUsbVendor g_uart_usb;
static tUsbVendorStats stats;

/* Monotonic counters; unsigned subtraction remains valid across wraparound. */
static uint8_t tx_ring[USB_RING_SIZE];
static uint32_t tx_head, tx_tail;
static uint16_t tx_remaining, tx_pending;
static uint8_t tx_packet[VENDOR_EPIN_SIZE];
static bool tx_active, tx_zlp, tx_inflight;
static uint8_t tx_seq;
static volatile bool producer_busy;

static uint8_t rx_ring[USB_RING_SIZE];
static uint32_t rx_head, rx_tail, rx_last_ms;
static uint8_t rx_payload[BSP_USB_VENDOR_MAX_PAYLOAD];
static uint8_t rx_expected_seq;
static bool rx_have_seq;

static uint16_t crc16_byte(uint16_t crc, uint8_t byte) {
    crc ^= (uint16_t)byte << 8;
    for (uint8_t bit = 0U; bit < 8U; ++bit)
        crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U)
                              : (uint16_t)(crc << 1);
    return crc;
}

static uint32_t irq_lock(void) {
    uint32_t state = __get_PRIMASK();
    __disable_irq();
    return state;
}

static void irq_unlock(uint32_t state) { __set_PRIMASK(state); }

static uint8_t rx_at(uint32_t offset) {
    return rx_ring[(rx_tail + offset) & USB_RING_MASK];
}

/* Caller serializes against USB callbacks. Packet buffer stays owned until
 * DataIn, including the ZLP that terminates an exact multiple of 64 bytes. */
static void tx_kick(void) {
    if (tx_inflight || !USBD_VENDOR_IsReady())
        return;
    if (!tx_active) {
        if (tx_head == tx_tail)
            return;
        tx_remaining = tx_ring[tx_tail & USB_RING_MASK];
        tx_remaining |= (uint16_t)tx_ring[(tx_tail + 1U) & USB_RING_MASK] << 8;
        tx_tail += 2U; /* Internal queue length is not transmitted. */
        tx_active = true;
        tx_zlp = (tx_remaining % VENDOR_EPIN_SIZE) == 0U;
    }
    tx_pending =
        tx_remaining > VENDOR_EPIN_SIZE ? VENDOR_EPIN_SIZE : tx_remaining;
    for (uint16_t i = 0U; i < tx_pending; ++i)
        tx_packet[i] = tx_ring[(tx_tail + i) & USB_RING_MASK];
    USBD_StatusTypeDef status = USBD_VENDOR_Transmit(tx_packet, tx_pending);
    if (status == USBD_OK)
        tx_inflight = true;
    else
        ++stats.tx_submit_errors; /* Keep bytes for the next SOF retry. */
}

void bsp_usb_vendor_on_tx_done(void) {
    if (!tx_inflight)
        return;
    tx_inflight = false;
    tx_tail += tx_pending;
    tx_remaining -= tx_pending;
    if (tx_remaining == 0U) {
        if (tx_pending == 0U || !tx_zlp)
            tx_active = false;
        /* Otherwise the next kick sends the terminating ZLP. */
    }
    tx_kick();
}

bool bsp_usb_vendor_ready(void) { return USBD_VENDOR_IsReady() != 0U; }

bool bsp_usb_vendor_send(void *handle, const uint8_t *data, uint16_t len) {
    /* Thread-context producers only: a higher-priority ISR could interrupt
     * DataIn midway through its queue update, even with USB IRQ disabled. */
    if (handle != &g_uart_usb || __get_IPSR() != 0U)
        return false;
    /* Claim a producer with a very short global critical section. Mask only
     * USB while encoding/copying: motor-control IRQs must remain serviceable.
     * Preserve the caller's original NVIC/PRIMASK state. */
    uint32_t key = irq_lock();
    if (producer_busy) {
        ++stats.tx_rejected;
        irq_unlock(key);
        return false;
    }
    producer_busy = true;
    uint32_t usb_enabled = NVIC_GetEnableIRQ(USB_LP_IRQn);
    NVIC_DisableIRQ(USB_LP_IRQn);
    __DSB();
    __ISB();
    irq_unlock(key);

    bool accepted = false;
    uint32_t wire_len = (uint32_t)len + BSP_USB_VENDOR_OVERHEAD;
    if (data != NULL && len != 0U && len <= BSP_USB_VENDOR_MAX_PAYLOAD &&
        bsp_usb_vendor_ready() &&
        USB_RING_SIZE - (tx_head - tx_tail) >= wire_len + 2U) {
        uint8_t header[BSP_USB_VENDOR_HEADER_SIZE] = {
            0xA5,
            0x5A,
            BSP_USB_VENDOR_VERSION,
            0U,
            tx_seq,
            (uint8_t)len,
            (uint8_t)(len >> 8),
        };
        uint32_t pos = tx_head;
        tx_ring[pos++ & USB_RING_MASK] = (uint8_t)wire_len;
        tx_ring[pos++ & USB_RING_MASK] = (uint8_t)(wire_len >> 8);
        uint16_t crc = 0xFFFFU;
        for (uint16_t i = 0U; i < sizeof(header); ++i) {
            tx_ring[pos++ & USB_RING_MASK] = header[i];
            crc = crc16_byte(crc, header[i]);
        }
        for (uint16_t i = 0U; i < len; ++i) {
            tx_ring[pos++ & USB_RING_MASK] = data[i];
            crc = crc16_byte(crc, data[i]);
        }
        tx_ring[pos++ & USB_RING_MASK] = (uint8_t)crc;
        tx_ring[pos++ & USB_RING_MASK] = (uint8_t)(crc >> 8);
        __DMB();
        tx_head = pos;
        ++tx_seq;
        ++stats.tx_messages;
        accepted = true;
        tx_kick();
    } else
        ++stats.tx_rejected;

    key = irq_lock();
    producer_busy = false;
    if (usb_enabled)
        NVIC_EnableIRQ(USB_LP_IRQn);
    irq_unlock(key);
    return accepted;
}

static void rx_parse(void) {
    while (rx_head - rx_tail >= 2U) {
        if (rx_at(0U) != 0xA5U || rx_at(1U) != 0x5AU) {
            ++rx_tail;
            continue;
        }
        if (rx_head - rx_tail < BSP_USB_VENDOR_HEADER_SIZE)
            return;
        uint16_t len = (uint16_t)rx_at(5U) | ((uint16_t)rx_at(6U) << 8);
        if (rx_at(2U) != BSP_USB_VENDOR_VERSION || rx_at(3U) != 0U ||
            len == 0U || len > BSP_USB_VENDOR_MAX_PAYLOAD) {
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
        uint8_t seq = rx_at(4U);
        if (rx_have_seq && seq != rx_expected_seq)
            ++stats.rx_sequence_errors;
        rx_expected_seq = (uint8_t)(seq + 1U);
        rx_have_seq = true;
        for (uint16_t i = 0U; i < len; ++i)
            rx_payload[i] = rx_at(BSP_USB_VENDOR_HEADER_SIZE + i);
        rx_tail += total;
        ++stats.rx_messages;
        if (g_uart_usb.rx_cb != NULL)
            g_uart_usb.rx_cb(g_uart_usb.rx_ctx, rx_payload, len);
        else
            ++stats.rx_no_callback;
    }
}

void bsp_usb_vendor_on_rx(const uint8_t *data, uint16_t len) {
    if (data == NULL || len == 0U)
        return; /* A short packet/ZLP is not an application message boundary. */
    if (len > USB_RING_SIZE - (rx_head - rx_tail)) {
        ++stats.rx_overflows;
        rx_tail = rx_head;
        rx_have_seq = false;
        if (len > USB_RING_SIZE)
            return;
    }
    for (uint16_t i = 0U; i < len; ++i)
        rx_ring[rx_head++ & USB_RING_MASK] = data[i];
    rx_last_ms = HAL_GetTick();
    rx_parse();
}

void bsp_usb_vendor_service(void) {
    if (rx_head != rx_tail && (uint32_t)(HAL_GetTick() - rx_last_ms) >=
                                  BSP_USB_VENDOR_RX_TIMEOUT_MS) {
        ++stats.rx_timeouts;
        /* A corrupt length can hide a later valid message. Skip one byte and
         * rescan before dropping whatever incomplete suffix is left. */
        ++rx_tail;
        rx_parse();
        rx_tail = rx_head;
    }
    tx_kick();
}

void bsp_usb_vendor_reset(void) {
    /* USB callback context. Retain callbacks and lifetime diagnostic counters.
     */
    tx_head = tx_tail = 0U;
    tx_remaining = tx_pending = 0U;
    tx_active = tx_zlp = tx_inflight = false;
    tx_seq = 0U;
    rx_head = rx_tail = 0U;
    rx_have_seq = false;
}

void bsp_usb_vendor_get_stats(tUsbVendorStats *out) {
    if (out == NULL)
        return;
    uint32_t key = irq_lock();
    *out = stats;
    irq_unlock(key);
}

static bool bsp_usb_vendor_open(void *handle) {
    return handle ==
           &g_uart_usb; /* Enumeration is started by MX_USB_Device_Init. */
}

static void bsp_usb_vendor_set_rx_cb(void *handle, uart_rx_done_cb cb,
                                     void *ctx) {
    (void)handle;
    uint32_t key = irq_lock();
    g_uart_usb.rx_ctx = ctx;
    g_uart_usb.rx_cb = cb;
    irq_unlock(key);
}

static bool bsp_usb_vendor_is_ready(void *handle) {
    return handle == &g_uart_usb && bsp_usb_vendor_ready();
}

static void bsp_usb_vendor_set_frame_cb(void *handle, usb_rx_frame_cb cb,
                                        void *ctx) {
    /* Both contracts consume a complete USB payload. They intentionally share
     * one callback slot, so a message never enters two independent parsers. */
    bsp_usb_vendor_set_rx_cb(handle, cb, ctx);
}

const tUsbOps usb_vendor_ops = {
    .open = bsp_usb_vendor_open,
    .send = bsp_usb_vendor_send,
    .set_rx_cb = bsp_usb_vendor_set_frame_cb,
    .is_ready = bsp_usb_vendor_is_ready,
};

static const tUsbDevHandle usb_vendor_handle = {
    .ops = &usb_vendor_ops,
    .dev = &g_uart_usb,
};

const tUsbDevHandle *Get_USB_VendorHandle(void) { return &usb_vendor_handle; }
