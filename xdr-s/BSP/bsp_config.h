//
// Created by wxd on 2026/11/24.
//

#ifndef XDR_S_BSP_CONFIG_H
#define XDR_S_BSP_CONFIG_H

#include "bsp_pwm.h"
#include "devices.h"

// USB配置
#define BSP_USB_VID XDR_S_APP_USB_VID
#define BSP_USB_PID XDR_S_APP_USB_PID
#define BSP_APP_SIZE (80U * 1024U)
#define BSP_USB_LANGID 1033U
#define BSP_USB_MANUFACTURER "wxd"
#define BSP_USB_PRODUCT "XDr-S"
#define BSP_USB_CONFIGURATION "Vendor HID Config"
#define BSP_USB_INTERFACE "Vendor HID Interface"
#define BSP_USB_REPORT_SIZE 64U
#define BSP_USB_REPORT_DESC_SIZE 27U
#define BSP_USB_POLL_INTERVAL 5U

// 板电气参数
#define MAX_CURRENT 60
#define MAX_VOLTAGE 34
#define MIN_VOLTAGE 14
#define MAX_TEMPERATURE 80

#define TIC_PWM 4249 //PWM arr
#define F_PWM PWM_FREQ// PWM频率
#define T_PWM (1/F_PWM) //pwm 周期
#define F_CON  F_PWM // 控制基频 (Hz)
#define T_CON  T_PWM // 控制周期 (s)

#endif //XDR_S_BSP_CONFIG_H
