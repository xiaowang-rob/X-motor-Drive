// UART通信端口驱动实现
// 实现UART协议帧收发、数据解析及VOFA+上位机数据传输功能

#include "uart_port.h"
#include "string.h"
#include "math_fast.h"

// 全局收发帧结构体
tUartFrame UsartRxFrame = {0}; ///< 接收帧缓冲区
tUartFrame UsartTxFrame = {0}; ///< 发送帧缓冲区

// DMA收发临时缓冲区
u8 _rx;                            ///< DMA接收单字节缓冲
u8 _tx;                            ///< DMA发送单字节缓冲（未使用）
u8 _tx_data[MAX_FRAME_LENGTH + 5]; ///< 帧组装缓冲区（含头尾校验）

// UART端口初始化
// 1. 初始化收发帧结构体，设置协议帧头尾
//       2. 启动DMA循环接收（每次接收1字节）
void uart_port_init(void)
{
    memset(&UsartRxFrame, 0, sizeof(tUartFrame));
    memset(&UsartTxFrame, 0, sizeof(tUartFrame));

    // 设置协议帧头尾标识
    UsartTxFrame.head = PACKET_HEAD;
    UsartTxFrame.tail = PACKET_TAIL;
    UsartRxFrame.head = PACKET_HEAD;
    UsartRxFrame.tail = PACKET_TAIL;

    // 启动DMA单字节循环接收
    bsp_uart_receive_dma(&_rx, 1);
}

// UART单字节发送
// data 待发送字节指针
void uart_send_byte(u8 *data)
{
    bsp_uart_transmit_dma(data, 1);
}

// UART多字节发送
// data 待发送数据缓冲区指针
// len  发送数据长度
void uart_send_data(u8 *data, u8 len)
{
    bsp_uart_transmit_dma(data, len);
}

// 发送协议数据帧
// id   消息ID
// data 待发送数据指针
// len  数据长度（1~MAX_FRAME_LENGTH）
// 帧格式：头(1)+ID(1)+长度(1)+数据(N)+校验(1)+尾(1)
//       校验算法：数据字节累加后取最低位
void uart_port_send_frame(u8 id, u8 *data, u8 len)
{
    if (len == 0)
        return;

    // 填充帧头信息
    UsartTxFrame.msgID = id;
    UsartTxFrame.len = len;

    // CRC8 校验    UsartTxFrame.check = crc8(data, len);

    // 复制数据到帧结构
    memcpy(UsartTxFrame.data, data, len);

    // 组装完整数据帧
    //     _tx_data[0] = UsartTxFrame.head;              // 帧头
    //     _tx_data[1] = UsartTxFrame.msgID;             // 消息ID
    //     _tx_data[2] = UsartTxFrame.len;               // 数据长度
    //     memcpy(_tx_data + 3, UsartTxFrame.data, len); // 数据域
    //     _tx_data[3 + len] = UsartTxFrame.check;       // 校验字节
    //     _tx_data[4 + len] = UsartTxFrame.tail;        // 帧尾

    // 启动DMA发送完整帧
    uart_send_data(_tx_data, 5 + len);
}

// 接收帧数据处理回调（弱定义）
// id   消息ID
// data 接收数据缓冲区
// len  有效数据长度
// 用户可在应用层重写此函数实现业务逻辑
__weak void uart_rx_frame_callback(u8 id, u8 *data, u8 len)
{
    return;
}

// 接收状态机变量
static bool get_head = false; ///< 帧头检测标志
static u16 check = 0;         ///< 校验和累加器
static u8 DataIndex = 0;      ///< 当前解析字节索引

// UART接收字节处理
// data 指向接收字节的指针
// 实现状态机协议解析：
//       1. 检测帧头(PACKET_HEAD)进入接收状态
//       2. 依次解析ID、长度、数据、校验、帧尾
//       3. 校验通过后调用用户处理函数（调试模式跳过校验）
//       4. 重新启动DMA接收下一字节
void uart_receive_byte(u8 *data)
{
    if (get_head)
    {
        // 检测帧尾
        if (*data == UsartRxFrame.tail)
        {
#ifndef __DEBUG__
            if (((u8)check & 0xff) == UsartRxFrame.check) // 校验通过
#endif
                uart_rx_frame_callback(UsartRxFrame.msgID, UsartRxFrame.data, UsartRxFrame.len);
            get_head = false; // 重置状态机
        }
        else
        {
            // 按协议顺序解析各字段
            if (DataIndex == 0)
                UsartRxFrame.msgID = *data; // 消息ID
            else if (DataIndex == 1)
                UsartRxFrame.len = *data; // 数据长度
            else if (DataIndex >= 2 && DataIndex < 2 + UsartRxFrame.len)
            {
                UsartRxFrame.data[DataIndex - 2] = *data; // 数据域
                check += *data;                           // 累加校验和
            }
            else if (DataIndex == 2 + UsartRxFrame.len)
            {
                UsartRxFrame.check = *data; // 校验字节
            }
            else if (DataIndex > 3 + UsartRxFrame.len)
            {
                get_head = false; // 超长帧丢弃
            }
            DataIndex++;
        }
    }
    else if (*data == UsartTxFrame.head) // 检测帧头
    {
        // 重置状态机
        get_head = true;
        DataIndex = 0;
        check = 0;
    }

    // 重新启动DMA接收下一字节
    bsp_uart_receive_dma(data, 1);
}

// UART接收完成中断回调
// huart UART句柄指针
// 仅处理USART1的接收中断，调用字节解析函数
void bsp_uart_rx_callback()
{
    uart_receive_byte(&_rx);
}

// VOFA+协议帧尾标识（固定4字节）
static u8 tail_bytes[4] = {0x00, 0x00, 0x80, 0x7F};

// VOFA+上位机浮点数据发送
// data  浮点数数组指针
// count 浮点数个数（1~8）
// 数据格式：连续float数据 + 4字节帧尾(0x00 0x00 0x80 0x7F)
//       适用于VOFA+上位机波形显示
void vofa_float_data_send(const float *data, u8 count)
{
    if (count == 0 || count > 8)
        return;

    // 复制浮点数据到发送缓冲区（小端格式）
    for (u8 i = 0; i < count; i++)
    {
        memcpy(&_tx_data[i * 4], &data[i], 4);
    }

    // 添加VOFA+协议帧尾
    memcpy(&_tx_data[count * 4], tail_bytes, 4);

    // 启动DMA发送完整数据包
    uart_send_data(_tx_data, count * 4 + 4);
}