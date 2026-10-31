#ifndef XDR_BSP_USB_VENDOR_H
#define XDR_BSP_USB_VENDOR_H

#include "uart_com.h"
#include "usb_com.h"

/* 驱动实例不透明；装配层通过类型化句柄访问。 */
typedef struct tUsbVendor tUsbVendor;
extern tUsbVendor g_uart_usb;
const tUartDevHandle *Get_MCU_USBHandle(void);
extern const tUartOps uart_usb_ops;
/* Native message ops: same handle, payload delivered without a UART wrapper.
 * One RX subscriber only: registering either ops replaces the previous one. */
extern const tUsbOps usb_vendor_ops;
const tUsbDevHandle *Get_USB_VendorHandle(void);

#define BSP_USB_VENDOR_MAX_PAYLOAD 4096U
#define BSP_USB_VENDOR_HEADER_SIZE 7U
#define BSP_USB_VENDOR_OVERHEAD 9U
#define BSP_USB_VENDOR_VERSION 1U
#define BSP_USB_VENDOR_RX_TIMEOUT_MS 1000U

/* Wire format, all multibyte integers little-endian (no C struct on wire):
 * A5 5A | version:u8=1 | flags:u8=0 | seq:u8 | length:u16 | payload | crc:u16
 * length: payload bytes, 1..4096. CRC16/CCITT-FALSE over the seven-byte header
 * AND payload: poly=0x1021, init=0xFFFF, refin/refout=false, xorout=0.
 * seq: independent per direction, increments modulo 256 per message; starts at
 * zero on USB reset/configuration. RX gaps/duplicates are counted, not retried
 * or discarded. First valid RX sequence establishes synchronization.
 *
 * EP 0x01 OUT / 0x81 IN: Bulk, max packet 64 bytes. USB packet boundaries are
 * not message boundaries. RX is length-delimited; TX ends each message with a
 * short packet (ZLP if its wire length is a multiple of 64).
 *
 * Compatibility: payload is presently ONE complete existing UART frame,
 * unchanged, including its head/id/length/payload/CRC8/tail. The transport can
 * carry 4096B but the external UART parser/queue still impose smaller limits.
 * send=true means a private copy was queued, NOT delivered or acknowledged.
 * Full queue, invalid length, concurrent producer or unconfigured USB => false.
 * send is thread/main-loop context only, never call it from an ISR/RX callback.
 * Caller must retry when false. Reset/disconnect discards all pending messages.
 */
bool bsp_usb_vendor_send(void *handle, const uint8_t *data, uint16_t len);
bool bsp_usb_vendor_ready(void);

typedef struct
{
    uint32_t tx_messages; /* Queued, not necessarily delivered. */
    uint32_t tx_rejected;
    uint32_t tx_submit_errors;
    uint32_t rx_messages;
    uint32_t rx_crc_errors;
    uint32_t rx_header_errors;
    uint32_t rx_sequence_errors;
    uint32_t rx_overflows;
    uint32_t rx_timeouts;
    uint32_t rx_no_callback;
} tUsbVendorStats;
void bsp_usb_vendor_get_stats(tUsbVendorStats *stats);

/* USB class hooks: serialized in USB interrupt context, no retained RX pointer.
 * rx_cb runs in this context too; it must copy/enqueue data, never block.
 * service is called once per Full Speed SOF (1ms), not from the main loop. */
void bsp_usb_vendor_on_rx(const uint8_t *data, uint16_t len);
void bsp_usb_vendor_on_tx_done(void);
void bsp_usb_vendor_service(void);
void bsp_usb_vendor_reset(void);

#endif
