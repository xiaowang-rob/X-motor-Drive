#include "uart_com.h"
#include "math_fast.h"

// 空闲回调和完成定长接收回调
void uart_rx_done_cb(tUartDriver *uart, uint8_t *data, uint16_t len)
{
    if (QUEUE_STATUS_OK != queue_static_enqueue_bulk(&uart->rx_queue, data, len))
    {
        uart->tstate = DEV_BUSY;
        return;
    }
    uart->rstate = DEV_RUNNING;
}

// 初始化消息队列
bool uart_init(tUartDriver *uart, tUartDriverOps *ops,
               uint8_t *pkt_buf, uint8_t pkt_size, uint8_t head, uint8_t tail)
{
    if (!uart || !ops)
        return false;
    uart->ops = ops;
    uart->pkt_head = head;
    uart->pkt_tail = tail;
    uart->pkt_get_head = false;
    uart->pkt_get_tail = true;
    uart->ops->init();
    uart->ops->register_callback(uart_rx_done_cb);
    // 初始化消息队列
    if (QUEUE_STATUS_OK != message_init_queue(&uart->rx_queue, pkt_buf, pkt_size))
        return false;
    return true;
}

// 发送数据
bool uart_send(tUartDriver *uart, tUart_Frame *frame)
{
    if (!uart || !frame)
        return false;
    uint16_t frame_len = frame->data_len + 5; // head id len data check tail
    uint8_t tx_buf[frame_len];
    tx_buf[0] = uart->pkt_head;
    tx_buf[1] = frame->id;
    tx_buf[2] = frame->data_len;
    memcpy(tx_buf + 3, frame->data, frame->data_len);
    tx_buf[frame_len - 1] = uart->pkt_tail;
    if (!uart->ops->send(tx_buf, frame_len))
    {
        uart->tstate = DEV_BUSY;
        return false;
    }
    uart->tstate = DEV_RUNNING;
    return true;
}

// 持续提取并返回数据帧 没有则返回NULL
bool uart_process_frame(tUartDriver *uart, tUart_Frame *frame)
{
    if (!uart)
        return false;
    if (queue_static_is_empty(&uart->rx_queue))
        return false;

    uint8_t rdata = 0;
    while (QUEUE_STATUS_EMPTY != queue_static_dequeue(&uart->rx_queue, rdata))
    {
        if (uart->pkt_get_tail)
        {
            if (rdata == uart->pkt_head)
            {
                uart->pkt_get_head = true;
                uart->pkt_get_tail = false;
                uart->index = 0;
            }
        }
        else
        {
            if (rdata == uart->pkt_tail)
            {
                uart->pkt_get_head = false;
                uart->pkt_get_tail = true;
                uint8_t check = frame->data[uart->rx_index - 1];
                frame->data_len = uart->index - 3;
                uart->index = 0;
                uart->pkt_get_head = false;
                uart->pkt_get_tail = true;
                if (check != math_fast_crc8(frame->data, frame->data_len))
                    continue; // 丢弃错误帧 重新提取下一帧
                else
                    return true; // 返回正确帧
            }
            if (0 == uart->index)
                frame.id = rdata;
            else if (1 == uart->index)
                frame.data_len = rdata;
            else
                frame.data[uart->index - 2] = rdata;

            uart->index++;
        }
    }
}
