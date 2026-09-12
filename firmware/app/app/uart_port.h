#ifndef __UART_PORT_H
#define __UART_PORT_H

#include "bsp_uart.h"
#include "protocol.h"

typedef struct
{
    u8 head;
    u8 msgID;
    u8 len;
    u8 data[MAX_FRAME_LENGTH];
    u8 check;
    u8 tail;
} tUartFrame;

// 函数声明
void uart_port_init();
void uart_port_send_data(u8 *data, u8 len);
void uart_port_send_frame(u8 id, u8 *data, u8 len);
void uart_rx_frame_callback(u8 id, u8 *data, u8 len);

// 发送多个浮点数（VOFA+ Float 格式）
void vofa_float_data_send(const float *data, u8 count);

#endif