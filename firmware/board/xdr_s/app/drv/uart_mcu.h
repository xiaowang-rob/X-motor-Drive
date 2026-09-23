#ifndef __UART_MCU_H
#define __UART_MCU_H

#include "uart_com.h"

// 驱动实例（布局私有：只暴露符号，装配层仅取地址）
typedef struct tUartMcu tUartMcu;
extern tUartMcu g_uart0;

// 驱动 ops（abs/uart_com.h 的 tUartOps）
extern const tUartOps uart_mcu_ops;

#endif // __UART_MCU_H
