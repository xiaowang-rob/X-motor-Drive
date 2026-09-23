// ============================================================
// uart_usb_cdc.c — USB CDC 虚拟串口驱动（板级）
//
// 接收链：usbd_cdc_if.c 的 CDC_Receive_FS（USER CODE）→ 本文件转发 → uart_com 入队
// 发送：CDC_Transmit_FS（总线忙时返回 false，由上层重试）
// ============================================================
#include "uart_usb_cdc.h"

#include "usbd_cdc_if.h"

// ---------- 实例 ----------
struct tUartUsb
{
    uart_rx_done_cb rx_cb; // 收字节回调（由 uart_com 注册）
    void *rx_ctx;          // 回调上下文（uart_com 实例）
};

tUartUsb g_uart_usb = {
    .rx_cb = NULL,
    .rx_ctx = NULL,
};

// USB 栈回调（usbd_cdc_if USER CODE 调用）→ 转发给 uart_com
static void usb_on_rx(uint8_t *buf, uint16_t len)
{
    if (g_uart_usb.rx_cb)
        g_uart_usb.rx_cb(g_uart_usb.rx_ctx, buf, len);
}

// ---- 驱动接口（tUartOps） ----

static bool uart_usb_open(void *handle)
{
    (void)handle; // USB 栈回调无实例参数，转发链固定在 g_uart_usb
    mcu_usb_register_rx_callback(usb_on_rx);
    return true;
}

static bool uart_usb_send(void *handle, const uint8_t *data, uint16_t len)
{
    (void)handle;
    if (!data || len == 0U)
        return false;
    return CDC_Transmit_FS((uint8_t *)data, len) == USBD_OK;
}

static void uart_usb_set_rx_cb(void *handle, uart_rx_done_cb cb, void *ctx)
{
    (void)handle;
    g_uart_usb.rx_cb = cb;
    g_uart_usb.rx_ctx = ctx;
}

// ---- 驱动出口 ----
const tUartOps uart_usb_ops = {
    .open = uart_usb_open,
    .send = uart_usb_send,
    .set_rx_cb = uart_usb_set_rx_cb,
};
