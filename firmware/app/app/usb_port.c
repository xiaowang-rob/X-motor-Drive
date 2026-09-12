// USB CDC通信端口驱动实现
// 实现USB协议帧收发、数据封装及错误重发机制

#include "usb_port.h"
#include "bsp_base.h"
#include "string.h"
#include "math_fast.h"
// USB协议帧全局变量
tUSB_Frame UsbTxFrame = {.head = USB_PACKET_HEAD, .tail = USB_PACKET_TAIL}; // 发送帧结构体
tUSB_Frame UsbRxFrame = {.head = USB_PACKET_HEAD, .tail = USB_PACKET_TAIL}; // 接收帧结构体

// 上拉 让上位机识别到USB口
void usb_init(void)
{
     bsp_usb_cs(true); // 使能USB时钟
}

// USB数据发送函数（带重发机制）
// data 待发送数据缓冲区指针
// len  发送数据长度
// true  发送成功
// false 发送失败（重发5次后仍失败）
// 1. 调用CDC_Transmit_FS进行底层发送
//       2. 发送忙时自动重发，最多重试5次
//       3. @warning 成功返回路径中trans_fault_tic清零语句位于return之后，实际不会执行
bool usb_send_data(u8 *data, u8 len)
{
    for (int i = 0; i <= 5; i++)
    {
        if (bsp_usb_cdc_transmit_fs(data, len))
        {
            return true;
        }
        if (i < 5)
        {
             bsp_delay(1); // 等待 1ms 再重试
        }
    }
    return false;
}

// USB协议帧发送函数
// id   消息ID
// data 待发送数据指针
// len  数据长度（1~MAX_USB_DATA_LEN）
// true  帧封装并发送成功
// false 发送失败或数据长度为0
// 帧格式：头(1)+ID(1)+长度(1)+数据(N)+校验(1)+尾(1)
//       校验算法：数据字节累加后取最低位
bool usb_send_frame(u8 id, u8 *data, u8 len)
{
    if (len == 0)
        return false;

    // CRC8 校验
    u8 check = crc8(data, len);

    // 填充发送帧结构
    UsbTxFrame.id = id;
    UsbTxFrame.len = len;
    memcpy(UsbTxFrame.data, data, len);
    UsbTxFrame.check = check;

    // 组装完整帧：head(1)+id(1)+len(1)+data(N)+check(1)+tail(1) = N+5字节
     UsbTxFrame.data[len] = UsbTxFrame.check;    // 校验字节
     UsbTxFrame.data[len + 1] = UsbTxFrame.tail; // 帧尾

    // 发送完整帧（包含帧头）
    return usb_send_data((u8 *)&UsbTxFrame, len + 5);
}

// USB接收帧数据处理回调（弱定义）
// id   消息ID
// data 接收数据缓冲区
// len  有效数据长度
// 用户可在应用层重写此函数实现业务逻辑
__weak void usb_rx_frame_callback(u8 id, u8 *data, u8 len)
{
    return;
}

// USB接收数据解析函数
// data 指向接收数据缓冲区的指针
// len  指向数据长度的指针（输入/输出参数）
// true  帧解析成功（无论校验是否通过）
// false 参数错误或校验失败
// 1. 验证帧头尾完整性
//       2. 校验数据域（调试模式可跳过校验）
//       3. 校验通过后调用用户处理回调
//       4. 帧格式：头(1)+ID(1)+长度(1)+数据(N)+校验(1)+尾(1)
bool bsp_usb_recv_byte(u8 *data, u8 *len)
{
    // 参数有效性检查
    if (data == NULL || len == NULL)
        return false;

    // 验证帧头尾
    if ((data[0] == UsbRxFrame.head) && (data[*len - 1] == UsbRxFrame.tail))
    {
        // 提取校验字节
        UsbRxFrame.check = data[*len - 2];

        // CRC8 校验（数据域从第3字节到校验字节前）
        u8 check = crc8(data + 3, *len - 5);

        // 校验比对
        if (check != UsbRxFrame.check)
            return false; // 校验失败

        // 校验通过，解析帧内容
         UsbRxFrame.id = data[1];                           // 消息ID
         UsbRxFrame.len = data[2];                          // 数据长度
         memcpy(UsbRxFrame.data, data + 3, UsbRxFrame.len); // 数据域

        // 调用用户处理回调
        usb_rx_frame_callback(UsbRxFrame.id, UsbRxFrame.data, UsbRxFrame.len);
    }
    return true;
}