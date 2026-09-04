#ifndef __USB_DRV_H
#define __USB_DRV_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// usb_drv.h — USB CDC 通讯底层驱动（usr/drv）
//
// 封装 USB_DEVICE 中间件 CDC 接口：
//   - 发送：CDC_Transmit_FS
//   - 接收：由 usbd_cdc_if.c 的 CDC_Receive_FS 调用 usb_drv_rx_hook 交付
//     （USB_DEVICE/App/usbd_cdc_if.c 的 USER CODE 区已接入，见该文件）
// ============================================================

// CDC 发送（总线忙返回 false）
bool usb_drv_transmit(uint8_t *data, uint16_t len);

// 接收数据回调（中断上下文，尽快拷贝/入队）
typedef void (*usb_rx_cb)(uint8_t *data, uint16_t len);
void usb_drv_register_rx(usb_rx_cb cb);

// 供 usbd_cdc_if.c 调用的接收钩子（勿直接调用）
void usb_drv_rx_hook(uint8_t *data, uint16_t len);

#endif // __USB_DRV_H
