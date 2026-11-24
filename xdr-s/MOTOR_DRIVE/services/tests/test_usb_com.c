#include "usb_com.h"
#include <assert.h>
#include <stdio.h>

static usb_rx_frame_cb rx_cb;
static void *rx_ctx;
static bool open_dev(void *dev) { return dev != NULL; }
static bool send_msg(void *dev, const uint8_t *data, uint16_t len) {
    return dev != NULL && data != NULL && len != 0;
}
static void set_rx(void *dev, usb_rx_frame_cb cb, void *ctx) {
    (void)dev;
    rx_cb = cb;
    rx_ctx = ctx;
}

int main(void) {
    static const tUsbOps ops = {open_dev, send_msg, set_rx, open_dev};
    int dev = 0;
    const tUsbDevHandle handle = {&ops, &dev};
    uint8_t slots[2 * (8 + 2)];
    const tUsbBuffer buf = {slots, 2, 8};
    tUsbDriver usb = {0};
    assert(usb_init(&usb, &handle, &buf));
    assert(usb_start(&usb));

    uint8_t output[7];
    tUsb_Frame frame = {.data = output, .capacity = sizeof(output)};
    assert(!usb_process_frame(&usb, &frame));
    const uint8_t msg[] = {0x42, 0x11, 0x22, 0x33};
    rx_cb(rx_ctx, msg, sizeof(msg));
    frame.capacity = 2;
    assert(!usb_process_frame(&usb, &frame));
    assert(frame.id == 0x42 && frame.data_len == 3);
    frame.capacity = sizeof(output);
    assert(usb_process_frame(&usb, &frame));
    assert(frame.id == 0x42 && frame.data_len == 3);
    assert(memcmp(output, msg + 1, 3) == 0);
    assert(!usb_process_frame(&usb, &frame));

    const uint8_t empty[] = {0x13};
    rx_cb(rx_ctx, empty, sizeof(empty));
    frame.data = NULL;
    frame.capacity = 0;
    assert(usb_process_frame(&usb, &frame));
    assert(frame.id == 0x13 && frame.data_len == 0);

    const uint8_t maximum[] = {0x99, 1, 2, 3, 4, 5, 6, 7};
    rx_cb(rx_ctx, maximum, sizeof(maximum));
    assert(!usb_process_frame(&usb, &frame));
    assert(frame.id == 0x99 && frame.data_len == 7);
    frame.data = output;
    frame.capacity = sizeof(output);
    assert(usb_process_frame(&usb, &frame));
    assert(memcmp(output, maximum + 1, 7) == 0);

    rx_cb(rx_ctx, msg, 0);
    rx_cb(rx_ctx, empty, sizeof(empty));
    rx_cb(rx_ctx, msg, sizeof(msg));
    rx_cb(rx_ctx, maximum, sizeof(maximum)); /* 队列满，不覆盖前两条。 */
    assert(atomic_load(&usb.rx_dropped) == 2);
    assert(usb_process_frame(&usb, &frame) && frame.id == 0x13);
    assert(usb_process_frame(&usb, &frame) && frame.id == 0x42);
    assert(!usb_process_frame(&usb, &frame));
    puts("usb_com tests passed");
    return 0;
}
