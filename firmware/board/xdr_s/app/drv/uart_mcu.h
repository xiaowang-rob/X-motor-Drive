#ifndef __UART_MCU_H
#define __UART_MCU_H

#include "uart_com.h"

// ============================================================
// uart_mcu.h — MCU 串口驱动（板级，DMA + 空闲中断）
//
// 实现 abs/uart_com.h 的 tUartOps，并导出驱动实例供装配层挂钩。
// ============================================================

// 驱动实例（布局私有：只暴露符号，装配层仅取地址）
typedef struct tUartMcu tUartMcu;
extern tUartMcu g_uart1;

// 驱动 ops（abs/uart_com.h 的 tUartOps）
extern const tUartOps uart_mcu_ops;

#endif // __UART_MCU_H
