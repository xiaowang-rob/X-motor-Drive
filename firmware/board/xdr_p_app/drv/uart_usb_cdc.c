// ============================================================
// uart_usb_cdc.c — USB CDC 虚拟串口驱动（板级）
//
// 接收链：usbd_cdc_if.c 的 CDC_Receive_FS（USER CODE）→ 本文件转发 → abs 层入队
// 发送：CDC_Transmit_FS（总线忙时返回 false，由上层重试）
//
// 说明：USB 栈的回调签名不带实例参数，故本驱动用文件内单例承接，
//       再经 rx_cb/rx_ctx 把数据送回 abs 层实例。
// ============================================================
#include "uart_drivers.h"

#include "usbd_cdc_if.h"

// ---------- 实例 handle ----------
typedef struct
{
    const tUartDriverOps *ops; // 该实例的操作表
    uart_rx_done_cb rx_cb;     // 收字节回调（由 abs 层注册）
    void *rx_ctx;              // 回调上下文（abs 层实例）
} tUartUsb;

// USB CDC 为单实例
static tUartUsb s_usb;

// USB 栈回调（usbd_cdc_if USER CODE 调用）→ 转发给 abs 层
static void usb_on_rx(uint8_t *buf, uint16_t len)
{
    if (s_usb.rx_cb)
        s_usb.rx_cb(s_usb.rx_ctx, buf, len);
}

static bool uart_usb_init(UartHandle h);
static bool uart_usb_send(UartHandle h, const uint8_t *data, uint16_t len);
static void uart_usb_register(UartHandle h, uart_rx_done_cb cb, void *ctx);

const tUartDriverOps uart_usb_ops = {
    .init = uart_usb_init,
    .send = uart_usb_send,
    .register_callback = uart_usb_register,
};

UartHandle uart_usb_get_handle(void)
{
    return (UartHandle)&s_usb;
}

// ---- ops 实现 ----

static bool uart_usb_init(UartHandle h)
{
    tUartUsb *inst = (tUartUsb *)h;
    if (!inst)
        return false;

    inst->ops = &uart_usb_ops;
    mcu_usb_register_rx_callback(usb_on_rx);
    return true;
}

static bool uart_usb_send(UartHandle h, const uint8_t *data, uint16_t len)
{
    (void)h;
    if (!data || len == 0U)
        return false;
    return CDC_Transmit_FS((uint8_t *)data, len) == USBD_OK;
}

static void uart_usb_register(UartHandle h, uart_rx_done_cb cb, void *ctx)
{
    tUartUsb *inst = (tUartUsb *)h;
    if (!inst)
        return;
    inst->rx_cb = cb;
    inst->rx_ctx = ctx;
}
