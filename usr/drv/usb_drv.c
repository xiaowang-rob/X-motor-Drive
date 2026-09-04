// ============================================================
// usb_drv.c — USB CDC 通讯底层驱动（usr/drv）
// ============================================================

#include "usb_drv.h"

// 中间件接口（不 include 其头，避免 include 路径耦合；签名见 usbd_cdc_if.h）
extern uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len);

static usb_rx_cb s_rx_cb = NULL;

bool usb_drv_transmit(uint8_t *data, uint16_t len)
{
    if (!data || len == 0U)
        return false;
    return CDC_Transmit_FS(data, len) == 0U; // USBD_OK
}

void usb_drv_register_rx(usb_rx_cb cb)
{
    s_rx_cb = cb;
}

void usb_drv_rx_hook(uint8_t *data, uint16_t len)
{
    if (s_rx_cb)
        s_rx_cb(data, len);
}
