#include "bl_comm.h"

#include "bl_iap.h"
#include "cmd_list.h"
#include "uart_com.h"
#include "usb_com.h"

/* USB 完整业务报文：[command:u8][payload...]；外层 CRC/分片由 BSP 负责。
 * UART 使用 uart_com 的帧结构。回复始终留在请求通道。
 * 未成功入发送队列的回复保留重试，不重复执行擦写命令。 */
static tUart_Frame s_uart_reply;
static bool s_uart_pending;
static uint8_t s_usb_reply[129];
static uint16_t s_usb_reply_len;
static bool s_usb_pending;

bool bl_comm_init(void) {
    s_uart_pending = false;
    s_usb_pending = false;
    s_usb_reply_len = 0;
    return uart_start(&g_uart, UART_PACKET_HEAD, UART_PACKET_TAIL) &&
           usb_start(&g_usb);
}

void bl_comm_poll(void) {
    if (s_uart_pending && uart_send(&g_uart, &s_uart_reply))
        s_uart_pending = false;
    if (s_usb_pending && usb_send(&g_usb, s_usb_reply, s_usb_reply_len))
        s_usb_pending = false;

    tUart_Frame frame;
    if (!s_uart_pending && uart_process_frame(&g_uart, &frame)) {
        uint16_t len = 0;
        s_uart_reply.id = frame.id;
        (void)bl_iap_command(frame.id, frame.data, frame.data_len,
                             s_uart_reply.data, &len);
        s_uart_reply.data_len = (uint8_t)len;
        s_uart_pending = len != 0;
    }
    uint8_t message[129];
    tUsb_Frame usb_frame = {.data = message, .capacity = sizeof(message)};
    if (!s_usb_pending && usb_process_frame(&g_usb, &usb_frame) &&
        usb_frame.data_len > 0) {
        uint16_t len = 0;
        s_usb_reply[0] = message[0];
        (void)bl_iap_command(message[0], message + 1, usb_frame.data_len - 1,
                             s_usb_reply + 1, &len);
        s_usb_reply_len = len + 1;
        s_usb_pending = len != 0;
    }
}
