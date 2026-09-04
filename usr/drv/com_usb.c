// ============================================================
// usb_drv.c — USB CDC 通讯底层驱动（usr/drv）
// 直接去usbd_cdc_if中注册回调
// ============================================================

#include "usb_drv.h"
#include "usbd_cdc_if.h"

static usb_rx_cb s_rx_cb = NULL;

bool usb_drv_transmit(uint8_t *data, uint16_t len)
{
    if (!data || len == 0U)
        return false;
    return CDC_Transmit_FS(data, len) == 0U; // USBD_OK
}

void usb_drv_register_rx(usb_rx_cb cb)
{
    mcu_usb_register_rx_callback(cb);
}
