#ifndef __BSP_CFG_H
#define __BSP_CFG_H

#include "device.h"
#include "parameter_list.h"

// USB vendor HID 配置（设备身份与 report 传输参数）
#define BSP_USB_VID 0x0483
#define BSP_USB_PID 0x5750
#define BSP_USB_LANGID 1033U
#define BSP_USB_MANUFACTURER "wxd"
#define BSP_USB_PRODUCT "XDr-S"
#define BSP_USB_CONFIGURATION "Vendor HID Config"
#define BSP_USB_INTERFACE "Vendor HID Interface"
#define BSP_USB_REPORT_SIZE 64U
#define BSP_USB_REPORT_DESC_SIZE 27U
#define BSP_USB_POLL_INTERVAL 5U

#include "bus_com.h"
#include "encoder.h"
#include "flash.h"
#include "iap.h"
#include "led.h"
#include "pwm.h"
#include "sample.h"
#include "uart_com.h"

// ---------- 固件信息 ----------
#define AUTHOR "wxd"
#define BOARD_NAME "XDr-S"
#define BOARD_VERSION "V2.1"
#define FIRM_VERSION "260626" // 这个由编译脚本更新
#define XDR_VERSION_STR (BOARD_NAME "_" BOARD_VERSION "_" FIRM_VERSION)

#define MAX_CURRENT 60
#define MAX_VOLTAGE 34
#define MIN_VOLTAGE 20
#define MAX_TEMPERATURE 80

extern const uint32_t TIC_PWM; // PWM计数值
extern const float F_PWM;      // PWM频率
extern const float T_PWM;      // PWM周期
extern const float F_CON;      // 控制基频 (Hz)
extern const float T_CON;      // 控制周期 (s)
extern const float T_SAMPLE;   // 采样时间
extern const float T_NOISE;    // 开关噪声时间
extern const float T_DIED;     // 死区时间

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

// ---- 存储 ----
extern tFlash g_flash; // 参数/日志区（MCU 内部 Flash 上的日志式单元）
extern tIAP g_iap;     // 固件升级（BL/APP 分区 + 平台跳转）

// ---- 功率级 ----
extern tGateDrv g_gate; // 功率级（PWM + 12V + 节拍中断）

// ---- 状态反馈 ----
#define LED_ACTIVE

extern tLed g_led; // 板载 LED0

// #define RGB_ACTIVE

// ---- 通讯 ----
extern tBusDriver g_can;   // 总线式（FDCAN）
extern tUartDriver g_uart; // MCU 串口
extern tUartDriver g_usb;  // USB vendor HID，保留 UART 帧适配

// ---- 传感器 ----
extern tEncoder g_enc; // 编码器
extern tSense g_sense; // 电流 / 母线 / 温度采样

// 装配并启动全板设备
bool bsp_base_init(void);

// 编码器初始化
bool bsp_enc_init(eObsList enc, eSpiEncList spi_enc);

#endif // __BSP_CFG_H
