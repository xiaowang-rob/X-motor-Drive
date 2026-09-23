#ifndef __UART_USB_CDC_H
#define __UART_USB_CDC_H

#include "uart_com.h"

// ============================================================
// uart_usb_cdc.h — USB CDC 虚拟串口驱动（板级）
//
// 实现 abs/uart_com.h 的 tUartOps，并导出驱动实例供装配层挂钩。
// 说明：USB 栈的回调签名不带实例参数，实例内部保存"转发到哪"，
//       USB 侧回调固定走本实例。
// ============================================================

// 驱动实例（布局私有：只暴露符号，装配层仅取地址）
typedef struct tUartUsb tUartUsb;
extern tUartUsb g_uart_usb;

// 驱动 ops（abs/uart_com.h 的 tUartOps）
extern const tUartOps uart_usb_ops;

#endif // __UART_USB_CDC_H
