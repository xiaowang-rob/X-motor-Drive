#include "usb_com.h"

/* ISR 中的 atomic 操作不能依赖锁或 libatomic。 */
_Static_assert(ATOMIC_INT_LOCK_FREE == 2, "USB RX requires lock-free unsigned atomics");

static void usb_on_rx(void *ctx, const uint8_t *data, uint16_t len) {
    tUsbDriver *usb = (tUsbDriver *)ctx;
    if (usb == NULL || !usb->started)
        return;
    unsigned write = atomic_load_explicit(&usb->rx_write, memory_order_relaxed);
    unsigned read = atomic_load_explicit(&usb->rx_read, memory_order_acquire);
    if (data == NULL || len == 0U || len > usb->rx_payload_size ||
        (unsigned)(write - read) >= usb->rx_slot_count) {
        atomic_fetch_add_explicit(&usb->rx_dropped, 1U, memory_order_relaxed);
        atomic_store_explicit(&usb->rstate, DEV_BUSY, memory_order_relaxed);
        return;
    }
    size_t stride = (size_t)usb->rx_payload_size + 2U;
    uint8_t *slot = usb->rx_storage + (write & (usb->rx_slot_count - 1U)) * stride;
    slot[0] = (uint8_t)len;
    slot[1] = (uint8_t)(len >> 8);
    memcpy(slot + 2U, data, len);
    atomic_fetch_add_explicit(&usb->rx_received, 1U, memory_order_relaxed);
    atomic_store_explicit(&usb->rstate, DEV_RUNNING, memory_order_relaxed);
    atomic_store_explicit(&usb->rx_write, write + 1U, memory_order_release);
}

bool usb_init(tUsbDriver *usb, const tUsbDevHandle *handle, const tUsbBuffer *buf) {
    if (!usb || !handle || !handle->ops || !handle->dev)
        return false;
    usb->handle = handle;

    if (usb == NULL || buf == NULL || usb->handle == NULL || usb->handle->ops == NULL ||
        usb->handle->ops->open == NULL || usb->handle->ops->send == NULL ||
        usb->handle->ops->set_rx_cb == NULL || usb->handle->ops->is_ready == NULL ||
        buf->rx_storage == NULL || buf->rx_slot_count < 2U ||
        (buf->rx_slot_count & (buf->rx_slot_count - 1U)) != 0U ||
        buf->rx_payload_size == 0U || buf->rx_payload_size > USB_MAX_PKT_SIZE)
        return false;
    usb->rx_storage = buf->rx_storage;
    usb->rx_slot_count = buf->rx_slot_count;
    usb->rx_payload_size = buf->rx_payload_size;
    atomic_init(&usb->rx_write, 0U);
    atomic_init(&usb->rx_read, 0U);
    atomic_init(&usb->rx_received, 0U);
    atomic_init(&usb->rx_dropped, 0U);
    atomic_init(&usb->rstate, DEV_OFFLINE);
    usb->tstate = DEV_OFFLINE;
    usb->started = false;
    usb->initialized = true;
    return true;
}

bool usb_start(tUsbDriver *usb) {
    if (usb == NULL || !usb->initialized)
        return false;
    if (usb->started)
        return true;
    if (!usb->handle->ops->open(usb->handle->dev))
        return false;
    /* 注册回调之前发布全部实例字段；驱动负责与其 IRQ 互斥。 */
    usb->started = true;
    usb->tstate = DEV_ONLINE;
    atomic_store_explicit(&usb->rstate, DEV_ONLINE, memory_order_relaxed);
    usb->handle->ops->set_rx_cb(usb->handle->dev, usb_on_rx, usb);
    return true;
}

bool usb_is_ready(const tUsbDriver *usb) {
    return usb != NULL && usb->initialized && usb->started &&
           usb->handle->ops->is_ready(usb->handle->dev);
}

bool usb_send(tUsbDriver *usb, const uint8_t *data, uint16_t len) {
    if (usb == NULL || !usb->initialized || !usb->started || data == NULL ||
        len == 0U || len > USB_MAX_PKT_SIZE)
        return false;
    if (!usb_is_ready(usb)) {
        usb->tstate = DEV_OFFLINE;
        return false;
    }
    if (!usb->handle->ops->send(usb->handle->dev, data, len)) {
        usb->tstate = DEV_BUSY;
        return false;
    }
    usb->tstate = DEV_RUNNING;
    return true;
}

bool usb_process_frame(tUsbDriver *usb, tUsb_Frame *frame) {
    if (frame == NULL)
        return false;
    frame->data_len = 0U;
    if (usb == NULL || !usb->initialized || !usb->started)
        return false;
    unsigned read = atomic_load_explicit(&usb->rx_read, memory_order_relaxed);
    unsigned write = atomic_load_explicit(&usb->rx_write, memory_order_acquire);
    if (read == write)
        return false;
    size_t stride = (size_t)usb->rx_payload_size + 2U;
    const uint8_t *slot = usb->rx_storage + (read & (usb->rx_slot_count - 1U)) * stride;
    uint16_t len = (uint16_t)slot[0] | ((uint16_t)slot[1] << 8);
    frame->data_len = len;
    if (frame->data == NULL || frame->capacity < len)
        return false; /* 不消费，调用方可以提供更大的输出缓冲重试。 */
    memcpy(frame->data, slot + 2U, len);
    atomic_store_explicit(&usb->rx_read, read + 1U, memory_order_release);
    return true;
}
