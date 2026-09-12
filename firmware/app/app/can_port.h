#ifndef __CAN_PORT_H
#define __CAN_PORT_H

#include "bsp_can.h"
#include "queue.h"

typedef struct
{
    u32 id;
    u8 err_count;
    bool queue_flag;
    u8 queue_head;
    u8 queue_tail;
    tStaticQueue rx_queue;
} tCAN_handle;

// 函数声明
bool can_port_init(u32 CAN_ID, bool canQUEUE);
bool can_set_config(u32 CAN_ID, bool canQUEUE);
bool can_send_data(u8 *msg, u8 len);
void can_rx_data_callback(u8 *RxData, u8 len);
void can_rx_error_callback(void);
void can_queue_data_deal();

#endif