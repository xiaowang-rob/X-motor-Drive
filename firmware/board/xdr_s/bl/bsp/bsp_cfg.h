#ifndef __BSP_CFG_H
#define __BSP_CFG_H

#include "led.h"
#include "storage.h"
#include "uart_com.h"
#include "usb_com.h"

// USB vendor HID 配置（设备身份与 report 传输参数）
#include "devices.h"
#define BSP_USB_VID XDR_S_BL_USB_VID
#define BSP_USB_PID XDR_S_BL_USB_PID
#define BSP_USB_LANGID 1033U
#define BSP_USB_MANUFACTURER "wxd"
#define BSP_USB_PRODUCT "XDr-S-bl"
#define BSP_USB_CONFIGURATION "Vendor HID Config"
#define BSP_USB_INTERFACE "Vendor HID Interface"
#define BSP_USB_REPORT_SIZE 64U
#define BSP_USB_REPORT_DESC_SIZE 27U
#define BSP_USB_POLL_INTERVAL 5U

// ---------- 硬件信息 ----------
#define HW_BOARD_NAME "XDr-S"
#define HW_VERSION_MAJOR 1
#define HW_VERSION_MINOR 0
#define HW_VERSION_PATCH 0

// ============================================================
// bsp_cfg.h — 本板装配层：全板设备对象与启动入口
//
// 装配范式：每个设备单独定义业务对象，初始化时传入 BSP 的
//           类型化句柄（ops + dev），见 bsp_cfg.c。
// 分层职责：
//   - 本层是**唯一** include 板级驱动出口头（xxx_drv / 芯片驱动）的地方；
//   - 业务层只引用下面这些设备对象，不触碰驱动；
//   - abs 层只认类型化句柄，不解析不透明 dev。
// ============================================================

// 装配并启动全板设备
bool bsp_base_init(void);

#endif // __BSP_CFG_H
