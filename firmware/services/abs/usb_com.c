#include "usb_com.h"

/* ISR 中的 atomic 操作不能依赖锁或 libatomic。 */
_Static_assert(ATOMIC_INT_LOCK_FREE == 2,
               "USB RX requires lock-free unsigned atomics");

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
    uint8_t *slot =
        usb->rx_storage + (write & (usb->rx_slot_count - 1U)) * stride;
    slot[0] = (uint8_t)len;
    slot[1] = (uint8_t)(len >> 8);
    memcpy(slot + 2U, data, len);
    atomic_fetch_add_explicit(&usb->rx_received, 1U, memory_order_relaxed);
    atomic_store_explicit(&usb->rstate, DEV_RUNNING, memory_order_relaxed);
    atomic_store_explicit(&usb->rx_write, write + 1U, memory_order_release);
}

bool usb_init(tUsbDriver *usb, const tUsbDevHandle *handle,
              const tUsbBuffer *buf) {
    if (!usb || !handle || !handle->ops || !handle->dev)
        return false;
    usb->handle = handle;

    if (usb == NULL || buf == NULL || usb->handle == NULL ||
        usb->handle->ops == NULL || usb->handle->ops->open == NULL ||
        usb->handle->ops->send == NULL || usb->handle->ops->set_rx_cb == NULL ||
        usb->handle->ops->is_ready == NULL || buf->rx_storage == NULL ||
        buf->rx_slot_count < 2U ||
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

bool usb_send(tUsbDriver *usb, uint8_t id, const uint8_t *data, uint16_t len) {
    if (usb == NULL || !usb->initialized || !usb->started || data == NULL ||
        len > USB_MAX_PKT_SIZE)
        return false;
    if (!usb_is_ready(usb)) {
        usb->tstate = DEV_OFFLINE;
        return false;
    }
    if (!usb->handle->ops->send(usb->handle->dev, id, data, len)) {
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
    const uint8_t *slot =
        usb->rx_storage + (read & (usb->rx_slot_count - 1U)) * stride;
    uint16_t len = (uint16_t)slot[0] | ((uint16_t)slot[1] << 8);
    frame->id = slot[2];
    frame->data_len = (uint16_t)(len - 1U);

    /* 缓冲不足时不释放槽，允许调用方使用更大缓冲重试。 */
    if (frame->data_len != 0U &&
        (frame->data == NULL || frame->capacity < frame->data_len)) {
        return false;
    }

    /* 复制期间尚未推进 rx_read，中断不能重用这个槽。 */
    if (frame->data_len != 0U) {
        memcpy(frame->data, slot + 3U, frame->data_len);
    }

    /* 数据已经复制到调用方缓冲，此时才能释放槽。 */
    atomic_store_explicit(&usb->rx_read, read + 1U, memory_order_release);
    return true;
}
