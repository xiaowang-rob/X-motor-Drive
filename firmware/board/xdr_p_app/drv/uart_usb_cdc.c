// ============================================================
// uart_usb_cdc.c — USB CDC 虚拟串口驱动（板级）
//
// 接收链：usbd_cdc_if.c 的 CDC_Receive_FS（USER CODE）→ 本文件转发 → abs 层入队
// 发送：CDC_Transmit_FS（总线忙时返回 false，由上层重试）
//
// 说明：USB 栈的回调签名不带实例参数，故本驱动用文件内单例承接，
//       再经回调指针把数据送回 abs 层实例。
// 本文件实现 app/abs/uart_com_board.h 的 USB 路由（由 board_uart.c 分派）。
// ============================================================
#include "uart_usb_cdc.h"

#include "usbd_cdc_if.h"

static uart_rx_done_cb s_rx_cb = NULL; // 收字节回调（由 abs 层注册）
static void *s_rx_ctx = NULL;          // 回调上下文（abs 层实例）

// USB 栈回调（usbd_cdc_if USER CODE 调用）→ 转发给 abs 层
static void usb_on_rx(uint8_t *buf, uint16_t len)
{
    if (s_rx_cb)
        s_rx_cb(s_rx_ctx, buf, len);
}

// ---- 板级钩子实现（USB 路由） ----

bool uart_usb_open(void)
{
    mcu_usb_register_rx_callback(usb_on_rx);
    return true;
}

bool uart_usb_send(const uint8_t *data, uint16_t len)
{
    if (!data || len == 0U)
        return false;
    return CDC_Transmit_FS((uint8_t *)data, len) == USBD_OK;
}

void uart_usb_register_cb(uart_rx_done_cb cb, void *ctx)
{
    s_rx_cb = cb;
    s_rx_ctx = ctx;
}
