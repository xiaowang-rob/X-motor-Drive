#ifndef XDR_BSP_USB_H
#define XDR_BSP_USB_H

#include <stdbool.h>

void bsp_usb_irq_handler(void);
void bsp_usb_task(void);

/* 设备模式在启动 USB 前选定。 */
bool bsp_usb_start(bool dfu_mode);

#endif
