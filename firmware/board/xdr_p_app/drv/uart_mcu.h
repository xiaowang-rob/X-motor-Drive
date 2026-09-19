#ifndef XDR_BOARD_DRV_UART_MCU_H
#define XDR_BOARD_DRV_UART_MCU_H

#include "uart_com.h"

// MCU 串口（DMA + 空闲中断）—— 板级钩子的具体实现之一
bool uart_mcu_open(void);
bool uart_mcu_send(const uint8_t *data, uint16_t len);
void uart_mcu_register_cb(uart_rx_done_cb cb, void *ctx);

#endif // XDR_BOARD_DRV_UART_MCU_H
