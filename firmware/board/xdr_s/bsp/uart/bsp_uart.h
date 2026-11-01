#ifndef __BSP_UART_H
#define __BSP_UART_H

#include "uart_com.h"

// ---------- 本板配置 ----------
#define MCU_UART USART2
#define MCU_UART_DMA DMA1
#define MCU_UART_DMA_CH LL_DMA_CHANNEL_3
#define MCU_UART_RX_SIZE 64U // 定长接受长度

// 驱动实例
const tUartDevHandle *Get_MCU_UsartHandle(void);

// 驱动 ops
extern const tUartOps mcu_uart_ops;

#endif // __UART_MCU_H
