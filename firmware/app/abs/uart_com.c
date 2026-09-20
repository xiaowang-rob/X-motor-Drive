#include "uart_com.h"

#include <string.h>

#include "crc.h"

// ============================================================
// uart_com.c — 串行式通信业务对象（abs，纯逻辑）
//
// 驱动只负责"收到字节块就回调"，本层把字节入队并做帧解析：
//   帧格式：[head][id][len][payload...][crc][tail]
//   crc = crc8(id, len, payload...)
// 硬件动作经 ops + handle（装配时挂）直达驱动。
// 帧解析 / 发送缓冲均由调用方提供（见 tUartBuffer）。
// ============================================================

// 收字节回调（中断上下文）：ctx 为 uart_init 时绑定的 tUartDriver 实例
static void uart_on_rx_data(void *ctx, const uint8_t *data, uint16_t len)
{
    tUartDriver *uart = (tUartDriver *)ctx;
    if (!uart || !data || len == 0U)
        return;

    if (QUEUE_STATUS_OK != queue_static_enqueue_bulk(&uart->rx_queue, data, len))
    {
        uart->rstate = DEV_BUSY; // 队列满：丢弃本次收到的字节
        return;
    }
    uart->rstate = DEV_RUNNING;
}

bool uart_init(tUartDriver *uart, const tUartBuffer *buf)
{
    if (!uart || !buf || !uart->ops || !uart->handle)
        return false;
    if (!buf->rx_queue_buf || buf->rx_queue_size == 0U || !buf->frame_buf || !buf->tx_buf)
        return false;

    // 只初始化业务字段；ops / handle 是装配结果，不在本层改动
    uart->pkt_head = 0U;
    uart->pkt_tail = 0U;
    uart->in_frame = false;
    uart->rx_index = 0U;
    uart->frame_buf = buf->frame_buf;
    uart->tx_buf = buf->tx_buf;
    uart->tstate = DEV_OFFLINE;
    uart->rstate = DEV_OFFLINE;

    if (QUEUE_STATUS_OK != queue_static_init(&uart->rx_queue, buf->rx_queue_buf, buf->rx_queue_size))
        return false;

    return true;
}

bool uart_start(tUartDriver *uart, uint8_t head, uint8_t tail)
{
    if (!uart || !uart->ops || !uart->ops->open || !uart->ops->set_rx_cb)
        return false;

    uart->pkt_head = head;
    uart->pkt_tail = tail;

    if (!uart->ops->open(uart->handle))
        return false;

    // 注册回调并把本实例作为 ctx 回传
    uart->ops->set_rx_cb(uart->handle, uart_on_rx_data, uart);

    uart->tstate = DEV_ONLINE;
    uart->rstate = DEV_ONLINE;
    return true;
}

bool uart_send(tUartDriver *uart, const tUart_Frame *frame)
{
    if (!uart || !frame || !uart->ops || !uart->ops->send)
        return false;
    if (frame->data_len > UART_MAX_PKT_SIZE)
        return false;

    // [head][id][len][payload...][crc][tail]
    // 用调用方提供的持久缓冲组帧：DMA 发送期间该缓冲必须保持有效
    uint8_t *tx_buf = uart->tx_buf;
    uint16_t len = frame->data_len;
    uint16_t total = (uint16_t)len + 5U;

    tx_buf[0] = uart->pkt_head;
    tx_buf[1] = frame->id;
    tx_buf[2] = (uint8_t)len;
    memcpy(&tx_buf[3], frame->data, len);
    tx_buf[3U + len] = crc8(&tx_buf[1], (uint16_t)(len + 2U)); // id + len + payload
    tx_buf[4U + len] = uart->pkt_tail;

    if (!uart->ops->send(uart->handle, tx_buf, total))
    {
        uart->tstate = DEV_BUSY;
        return false;
    }
    uart->tstate = DEV_RUNNING;
    return true;
}

bool uart_process_frame(tUartDriver *uart, tUart_Frame *frame)
{
    if (!uart || !frame || !uart->frame_buf)
        return false;

    const uint16_t frame_cap = (uint16_t)UART_MAX_PKT_SIZE + 4U;

    uint8_t byte;
    while (QUEUE_STATUS_OK == queue_static_dequeue(&uart->rx_queue, &byte))
    {
        if (!uart->in_frame)
        {
            if (byte == uart->pkt_head)
            {
                uart->in_frame = true;
                uart->rx_index = 0U;
            }
            continue; // 未同步时丢弃非帧头字节
        }

        // 帧尾（且已收到 id/len/crc 的最短长度）
        if (byte == uart->pkt_tail && uart->rx_index >= 3U)
        {
            uint16_t total = (uint16_t)uart->frame_buf[1] + 3U; // id+len+payload+crc
            uart->in_frame = false;

            if (uart->rx_index != total || total > frame_cap)
                continue; // 长度不符：丢弃

            // 校验 id + len + payload
            if (crc8(uart->frame_buf, (uint16_t)(total - 1U)) != uart->frame_buf[total - 1U])
                continue; // CRC 错：丢弃

            frame->id = uart->frame_buf[0];
            frame->data_len = (uint8_t)(total - 3U);
            memcpy(frame->data, &uart->frame_buf[2], frame->data_len);
            uart->rstate = DEV_RUNNING;
            return true;
        }

        if (uart->rx_index < frame_cap)
            uart->frame_buf[uart->rx_index++] = byte;
        else
            uart->in_frame = false; // 超长：放弃本帧
    }

    return false;
}
