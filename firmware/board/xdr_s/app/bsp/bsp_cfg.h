#ifndef __BSP_CFG_H
#define __BSP_CFG_H

#include "device.h"

#include "bus_com.h"  // tBusDriver
#include "encoder.h"  // tEncoder
#include "flash.h"    // tFlash / tFlashUnit
#include "gate_drv.h" // tGateDrv
#include "iap.h"      // tIAP
#include "led.h"      // tLed
#include "sense.h"    // tSense
#include "uart_com.h" // tUartDriver

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

// ============================================================
// bsp_cfg.h — 本板装配层：全板设备对象与启动入口
//
// 装配范式：每个设备**单独定义**一个对象，并在定义处就地挂上
//           ops 与 handle（见 bsp_cfg.c）。没有聚合结构体。
// 分层职责：
//   - 本层是**唯一** include 板级驱动出口头（xxx_drv / 芯片驱动）的地方；
//   - 业务层只引用下面这些设备对象，不触碰驱动；
//   - abs 层只认 ops + 不透明 handle。
// ============================================================

// ---- 存储 ----
extern tFlash g_flash; // 参数/日志区（MCU 内部 Flash 上的日志式单元）
extern tIAP g_iap;     // 固件升级（BL/APP 分区 + 平台跳转）

// ---- 功率级 ----
extern tGateDrv g_gate; // 功率级（PWM + 12V + 节拍中断）

// ---- 状态反馈 ----
// 注：XDr-S 无 WS2812 灯串，装配层不提供 tRgb 设备。
extern tLed g_led_0; // 板载 LED0
extern tLed g_led_1; // 板载 LED1

// ---- 通讯 ----
extern tBusDriver g_can;   // 总线式（FDCAN）
extern tUartDriver g_uart; // MCU 串口
extern tUartDriver g_usb;  // USB CDC 虚拟串口

// ---- 传感器 ----
extern tEncoder g_enc_int; // 内置编码器
extern tEncoder g_enc_ext; // 外部编码器
extern tSense g_sense;     // 电流 / 母线 / 温度采样

// 装配并启动全板设备
bool bsp_base_init(void);

// 编码器初始化
bool bsp_enc_init(eEncoderMode int_mode, eEncoderMode ext_mode, eEncoderChip ext_chip);

#endif // __BSP_CFG_H
