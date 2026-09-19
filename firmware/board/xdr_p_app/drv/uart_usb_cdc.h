#ifndef XDR_BOARD_DRV_UART_USB_CDC_H
#define XDR_BOARD_DRV_UART_USB_CDC_H

#include "uart_com.h"

// USB CDC 虚拟串口 —— 板级钩子的具体实现之一
bool uart_usb_open(void);
bool uart_usb_send(const uint8_t *data, uint16_t len);
void uart_usb_register_cb(uart_rx_done_cb cb, void *ctx);

#endif // XDR_BOARD_DRV_UART_USB_CDC_H
