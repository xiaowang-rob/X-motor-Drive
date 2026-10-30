#ifndef XDR_USB_COM_H
#define XDR_USB_COM_H

#include "device.h"
#include <stdatomic.h>

/* USB 是完整报文通道，不复用 UART 的帧头/帧尾解析器。
 * BSP 负责线格式、CRC16、序号及 64B Bulk 分片；本层只保存完整 payload，
 * 不再次增加 CRC 或复制 UART 外层。payload 内的业务格式由调用方决定。
 *
 * 单生产者（USB RX 中断）/单消费者（主循环），每个实例缓冲由调用方提供。
 * init/start 仅在首次启用前调用；运行中不得重置实例或缓冲。
 * RX 回调只复制入队，业务在 usb_process_frame 之后的主循环中处理。
 */
#define USB_MAX_PKT_SIZE 4096U

typedef void (*usb_rx_frame_cb)(void *ctx, const uint8_t *data, uint16_t len);

typedef struct {
    bool (*open)(void *handle);
    /* 成功表示已复制并入驱动队列，不表示 USB 发送完成或业务 ACK。 */
    bool (*send)(void *handle, const uint8_t *data, uint16_t len);
    /* 驱动保证切换回调与中断互斥；RX 指针仅在回调期间有效。 */
    void (*set_rx_cb)(void *handle, usb_rx_frame_cb cb, void *ctx);
    bool (*is_ready)(void *handle);
} tUsbOps;

// BSP 导出的类型化句柄：操作表 + 驱动实例。
typedef struct {
    const tUsbOps *ops;
    void *dev;
} tUsbDevHandle;

typedef struct {
    uint8_t *rx_storage;    /* 大小至少 rx_slot_count * (rx_payload_size + 2)。 */
    uint16_t rx_slot_count; /* >=2，2 的幂；每槽一条报文，不覆盖未消费报文。 */
    uint16_t rx_payload_size; /* 1..USB_MAX_PKT_SIZE；按实际业务选择以节省 RAM。 */
} tUsbBuffer;

typedef struct {
    uint8_t *data;       /* 调用方的输出缓冲。 */
    uint16_t capacity;
    uint16_t data_len;   /* 成功时实际长度；缓冲不足时返回所需长度且不出队。 */
} tUsb_Frame;

typedef struct {
    const tUsbDevHandle *handle; // 驱动句柄，由 init 注入
    uint8_t *rx_storage;
    uint16_t rx_slot_count;
    uint16_t rx_payload_size;
    atomic_uint rx_write;
    atomic_uint rx_read;
    atomic_uint rx_received;
    atomic_uint rx_dropped; /* 队列满、非法长度或超出本实例槽容量。 */
    _Atomic(eDeviceStatus) rstate;
    eDeviceStatus tstate; /* 只由调用 usb_send 的主循环更新。 */
    bool initialized;
    bool started;
} tUsbDriver;

/* 由装配层传入句柄与缓冲，不引用 HAL、BSP 或全局设备。 */
bool usb_init(tUsbDriver *usb, const tUsbDevHandle *handle, const tUsbBuffer *buf);
bool usb_start(tUsbDriver *usb);
bool usb_is_ready(const tUsbDriver *usb);
/* 主循环/线程调用，禁止从 ISR 或 RX 回调调用；一次调用是一条完整消息。 */
bool usb_send(tUsbDriver *usb, const uint8_t *data, uint16_t len);
/* 非阻塞；false 表示空队列、参数无效或输出容量不足，不弹出半条报文。 */
bool usb_process_frame(tUsbDriver *usb, tUsb_Frame *frame);

#endif
