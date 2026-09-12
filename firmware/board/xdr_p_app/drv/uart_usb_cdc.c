// ============================================================
// usb_drv.c — USB CDC 通讯底层驱动（usr/drv）
// 直接去usbd_cdc_if中注册回调
// ============================================================
#include "uart_drivers.h"

#include "usbd_cdc_if.h"

#define USB_DP_GPIOx GPIOA
#define USB_DP_GPIOx_PIN GPIO_PIN_8

static uart_rx_done_cb usb_rx_cb = NULL;

void usb_rx_done_cb(uint8_t *data, uint16_t len)
{
    usb_rx_cb(data, len);
}
bool usb_init(void)
{
    // 拉高DP 让电脑识别
    HAL_GPIO_WritePin(USB_CS_GPIOx, USB_CS_GPIOx_PIN, GPIO_PIN_SET);
    // 注册回调
    mcu_usb_register_rx_callback(usb_rx_done_cb);
    return true; // USBD_OK
}
bool usb_transmit(uint8_t *data, uint16_t len)
{
    if (!data || len == 0U)
        return false;
    return CDC_Transmit_FS(data, len) == 0U; // USBD_OK
}

void usb_register_rx(uart_rx_done_cb cb)
{
    usb_rx_cb = cb;
}
