#ifndef XDR_APP_ABS_UART_COM_BOARD_H
#define XDR_APP_ABS_UART_COM_BOARD_H

#include "uart_com.h"

// ============================================================
// uart_com_board.h — 串行通信的板级钩子契约（abs 声明 / 板级实现）
//
// 实现 = board/<B>/drv/board_uart.c（按 eUartPort 分派到
//        uart_mcu.c / uart_usb_cdc.c）。
//
// 语义：
//   open         初始化该路串口并挂起接收
//   send         发送一段字节（DMA/CDC 忙时返回 false，由上层重试）
//   register_cb  注册收字节回调；板级在中断上下文调用 cb(ctx, data, len)
// ============================================================

bool uart_board_open(eUartPort port);
bool uart_board_send(eUartPort port, const uint8_t *data, uint16_t len);
void uart_board_register_cb(eUartPort port, uart_rx_done_cb cb, void *ctx);

#endif // XDR_APP_ABS_UART_COM_BOARD_H
