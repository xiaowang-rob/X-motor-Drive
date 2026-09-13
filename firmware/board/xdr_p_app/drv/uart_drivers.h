#ifndef __UART_DRIVERS_H
#define __UART_DRIVERS_H

#include "uart_com.h"

// ============================================================
// uart_drivers.h — 本板串行通讯驱动出口
//
//   - usart：MCU 串口（DMA + 空闲中断）
//   - usb  ：USB CDC 虚拟串口（注册进 usbd_cdc_if 的 USER CODE 回调）
//
// 每个驱动以"文件内静态 handle 实例"存在，本头只暴露 ops 与取实例函数。
// ============================================================

// ---- MCU 串口 ----
extern const tUartDriverOps uart_mcu_ops;
UartHandle uart_mcu_get_handle(void);

// ---- USB CDC 虚拟串口 ----
extern const tUartDriverOps uart_usb_ops;
UartHandle uart_usb_get_handle(void);

#endif // __UART_DRIVERS_H
