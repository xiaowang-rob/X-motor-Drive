#ifndef __COM_DRIVERS_H
#define __COM_DRIVERS_H

#include "message.h"

// 收帧回调（中断上下文，尽快拷贝）
typedef void (*can_rx_cb)(uint32_t id, const uint8_t *data, uint8_t len);

// 以标准帧 ID 配置过滤器并启动（可重复调用以改 ID）
bool can_drv_start(uint32_t std_id);

// 发送标准帧（阻塞等邮箱空出，超时返回 false）
bool can_drv_send(uint32_t id, const uint8_t *msg, uint8_t len);

// 注册收帧回调（NULL 注销）
void can_drv_register_rx(can_rx_cb cb);

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

// CDC 发送（总线忙返回 false）
bool usb_drv_transmit(uint8_t *data, uint16_t len);

// 接收数据回调（中断上下文，尽快拷贝/入队）这里和usb中回调函数类型相同，不行的话就在驱动做一次转发
typedef void (*usb_rx_cb)(uint8_t *data, uint16_t len);
void usb_drv_register_rx(usb_rx_cb cb);

#endif