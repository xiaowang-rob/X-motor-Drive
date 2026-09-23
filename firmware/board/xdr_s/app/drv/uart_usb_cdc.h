#ifndef __UART_USB_CDC_H
#define __UART_USB_CDC_H

#include "uart_com.h"

// 驱动实例（布局私有：只暴露符号，装配层仅取地址）
typedef struct tUartUsb tUartUsb;
extern tUartUsb g_uart_usb;

// 驱动 ops（abs/uart_com.h 的 tUartOps）
extern const tUartOps uart_usb_ops;

#endif // __UART_USB_CDC_H
