// ============================================================
// board_uart.c — 串行通信板级钩子的分派层
//
// 实现 app/abs/uart_com_board.h：按 eUartPort 把调用转给具体驱动
// （uart_mcu.c / uart_usb_cdc.c）——直接函数调用，无 ops 表。
// abs 层只认 uart_board_*，不感知具体是哪一个串口外设。
// ============================================================
#include "uart_com_board.h"

#include "uart_mcu.h"
#include "uart_usb_cdc.h"

bool uart_board_open(eUartPort port)
{
    switch (port)
    {
    case UART_PORT_MCU:
        return uart_mcu_open();
    case UART_PORT_USB:
        return uart_usb_open();
    default:
        return false;
    }
}

bool uart_board_send(eUartPort port, const uint8_t *data, uint16_t len)
{
    switch (port)
    {
    case UART_PORT_MCU:
        return uart_mcu_send(data, len);
    case UART_PORT_USB:
        return uart_usb_send(data, len);
    default:
        return false;
    }
}

void uart_board_register_cb(eUartPort port, uart_rx_done_cb cb, void *ctx)
{
    switch (port)
    {
    case UART_PORT_MCU:
        uart_mcu_register_cb(cb, ctx);
        break;
    case UART_PORT_USB:
        uart_usb_register_cb(cb, ctx);
        break;
    default:
        break;
    }
}
