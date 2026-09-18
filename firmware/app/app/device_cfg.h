#ifndef __DEVICE_CFG_H
#define __DEVICE_CFG_H

#include "device.h"

// ============================================================
// device_cfg.h — 组装层：全板设备的装配结果与唯一入口
//
// 分层职责：
//   - 本层是**唯一**允许 include 板级驱动头（*_drivers.h）的地方；
//   - 业务层只通过 g_dev 访问设备，不直接触碰驱动；
//   - ctl 层按 abs 类型接收指针（见 foc_init 的 tFocRefs），不依赖本头。
// ============================================================

#include "encoder.h"  // tEncoder
#include "sense.h"    // tSense
#include "gate_drv.h" // tGateDrv
#include "flash.h"    // tFlash / tFlashUnit
#include "iap.h"      // tIAP
#include "led.h"      // tLed / tRgb
#include "bus_com.h"  // tBusDriver
#include "uart_com.h" // tUartDriver

// 全板设备集合（由 device_cfg_init 一次性装配）
typedef struct
{
    // ---- 存储 ----
    tFlash flash; // 参数区（内部 MCU Flash 上的日志式单元）
    tIAP iap;     // 固件升级（BL/APP 分区 + 跳转）

    // ---- 功率级 ----
    tGateDrv gate; // 功率级（PWM + 12V + 节拍中断）

    // ---- 状态反馈 ----
    tLed led_0; // 板载 LED0
    tLed led_1; // 板载 LED1
    tRgb rgb;   // WS2812 灯珠串

    // ---- 通讯 ----
    tBusDriver can;   // 总线式（CAN）
    tUartDriver uart; // MCU 串口
    tUartDriver usb;  // USB CDC 虚拟串口

    // ---- 传感器 ----
    tEncoder enc_int; // 内置编码器
    tEncoder enc_ext; // 外部编码器
    tSense sense;     // 电流 / 母线 / 温度采样

    // ---- 装配结果 ----
    bool dev_ok;
} tDevBoard;

// 全局设备对象（装配完成后业务层直接使用）
extern tDevBoard g_dev;

// 装配并初始化全板设备（取驱动实例 → abs 对象 init）。
// 返回 true 表示关键设备（功率级 + 采样 + 编码器）装配成功。
bool device_cfg_init(void);

// FOC 节拍回调注册（转发给功率级驱动；TIM8 的中断由驱动持有）
void device_cfg_register_foc_isr(void (*sample_cb)(void), void (*ctrl_cb)(void));

#endif // __DEVICE_CFG_H
