#ifndef __UART_COM_H
#define __UART_COM_H

#include "device.h"
#include "queue.h"

// ============================================================
// uart_com.h — 串行式通信业务对象（abs）
//
// 串行通信是不定长字节流：驱动把收到的字节块回调进本层队列，
// 本层按"帧头 / 帧尾"提取并做 CRC8 校验后输出完整帧。
//
// 帧格式：[head][id][len][payload...][crc][tail]，crc = crc8(id, len, payload)
// 缓冲由**调用方提供**（见 tUartBuffer），实例本身不内嵌大数组。
//
// 接口形态：ops + handle（装配层在定义 tUartDriver 对象时挂上）。
// ============================================================

#define UART_MAX_PKT_SIZE 128 // 单帧最大载荷字节数

// 解析后的帧（过滤掉无效帧）
typedef struct
{
    uint8_t id;
    uint8_t data_len;
    uint8_t data[UART_MAX_PKT_SIZE];
} tUart_Frame;

// 缓冲描述（调用方提供）
typedef struct
{
    uint8_t *rx_queue_buf;  // 接收字节队列缓冲（容量须为 2 的幂）
    uint16_t rx_queue_size; // 队列容量字节数
    uint8_t *frame_buf;     // 帧解析缓冲，需 ≥ UART_MAX_PKT_SIZE + 4
    uint8_t *tx_buf;        // 发送组帧缓冲，需 ≥ UART_MAX_PKT_SIZE + 5
} tUartBuffer;

// 接收完成回调：驱动在中断上下文调用；ctx 为注册时传入的实例指针
typedef void (*uart_rx_done_cb)(void *ctx, const uint8_t *data, uint16_t len);

// 驱动接口（板级实现）；handle 为驱动实例
typedef struct
{
    bool (*open)(void *handle);                                            // 初始化该路并挂起接收
    bool (*send)(void *handle, const uint8_t *data, uint16_t len);         // 发送一段字节（忙则 false）
    void (*set_rx_cb)(void *handle, uart_rx_done_cb cb, void *ctx);        // 注册收字节回调
} tUartOps;

typedef struct
{
    const tUartOps *ops; // 驱动 ops（装配时挂）
    void *handle;        // 驱动实例（装配时挂）

    uint8_t pkt_head; // 帧头字节
    uint8_t pkt_tail; // 帧尾字节

    // ---- 帧解析状态 ----
    bool in_frame;      // 已收到帧头，正在收集
    uint16_t rx_index;  // 收集进度
    uint8_t *frame_buf; // 帧解析缓冲（调用方提供）
    uint8_t *tx_buf;    // 发送组帧缓冲（调用方提供，DMA 发送期间须持久）

    eDeviceStatus tstate; // 发送状态
    eDeviceStatus rstate; // 接收状态
    tStaticQueue rx_queue;
} tUartDriver;

// 启动对象：ops/handle 须已由装配层挂好；缓冲由调用方提供
bool uart_init(tUartDriver *uart, const tUartBuffer *buf);

// 启动接收并注册回调（运行期参数：帧头 / 帧尾）
bool uart_start(tUartDriver *uart, uint8_t head, uint8_t tail);

// 发送一帧（自动补帧头/长度/CRC/帧尾）
bool uart_send(tUartDriver *uart, const tUart_Frame *frame);

// 持续从队列解析并提取完整帧；成功返回 true 并填充 frame
bool uart_process_frame(tUartDriver *uart, tUart_Frame *frame);

#endif // __UART_COM_H
