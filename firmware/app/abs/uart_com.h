#ifndef __UART_COM_H
#define __UART_COM_H

#include "device.h"
#include "queue.h"
// 对串行式通信的抽象 一般以不定长字节传输 需要特定的协议解析和提取帧

#define UART_MAX_PKT_SIZE 128

// 解析后的帧结构 过滤掉无效帧
typedef struct
{
    uint8_t id;
    uint8_t data_len;
    uint8_t data[UART_MAX_PKT_SIZE];
} tUart_Frame;

// uart接收完成回调
typedef void (*uart_rx_done_cb)(uint8_t *data, uint16_t len);

typedef struct
{
    bool (*init)(void);
    bool (*send)(uint8_t *data, uint16_t length);
    void (*register_callback)(uart_rx_done_cb callback);

} tUartDriverOps;

typedef struct
{
    const tUartDriverOps *ops;
    uint8_t pkt_head;
    uint8_t pkt_tail;
    bool pkt_get_head;
    bool pkt_get_tail;
    uint8_t rx_index;

    eDeviceStatus tstate;
    eDeviceStatus rstate;
    tStaticQueue rx_queue;
} tUartDriver;

// 初始化消息队列
bool uart_init(tUartDriver *uart, tUartDriverOps *ops,
               uint8_t *pkt_buf, uint8_t pkt_size, uint8_t head, uint8_t tail);

// 发送数据
bool uart_send(tUartDriver *uart, tUart_Frame *frame);

// 持续提取并返回数据帧 没有则返回NULL
tUart_Frame uart_process_frame(tUartDriver *uart);

#endif