#ifndef __UART_DRV_H
#define __UART_DRV_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// uart_drv.h — 串口通讯底层驱动（usr/drv，v2 直连版）
//
// DMA 收发 + 中断完成回调；HAL_UART_RxCpltCallback 由本文件唯一持有。
// ============================================================

// 启动一轮 DMA 接收（收到 len 字节后回调）
void uart_drv_rx_start(uint8_t *buf, uint16_t len);

// DMA 发送
bool uart_drv_tx(uint8_t *data, uint16_t len);

// 接收完成回调（中断上下文）
typedef void (*uart_rx_done_cb)(uint16_t len);
void uart_drv_register_rx_done(uart_rx_done_cb cb);

#endif // __UART_DRV_H
