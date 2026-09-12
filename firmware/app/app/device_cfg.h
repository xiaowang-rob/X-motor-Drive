#ifndef __DEV_BOARD_H
#define __DEV_BOARD_H

#include "bus_drivers.h"
#include "encoder_drivers.h"
#include "flash_drivers.h"
#include "gate_drivers.h"
#include "led_drivers.h"
#include "sense_drivers.h"
#include "uart_drivers.h"

tFlash flash_mcu;
tIAP iap_app;
tLed led0;
tLed led1;
tRgb rgb;
tSense sen;

tGateDrv motor_drv;
tEncoder int_encoder;
tEncoder ext_encoder;

tBusDriver can;
tUartDriver usart;
tUartDriver usb;

// 全板设备集合（由 dev_board_init 一次性装配）
typedef struct
{
    const tTimeIf *time; // 板级时间基准

    // 编码器（电机位置/速度，FOC 使用）
    tEncoder enc;

    // 电流/电压/温度采样（FOC 电流环输入）
    tCurrentSense sense;

    // 灯效
    tLed led_can; // 板载 LED0（CAN 状态灯）
    tLed led_enc; // 板载 LED1（编码器状态灯）
    tRgb rgb;     // 板上 WS2812 灯珠串

    // 外部 SPI NOR Flash（日志/参数存储介质；芯片缺失时不可用）
    tFlashStore ext_flash;

    // 内部 MCU Flash：参数区日志单元 + IAP 分区表
    tFlashStore param_flash; // 参数区（board.h PARAMETER_LOAD_ADDR）
    tFlashIAP iap;           // BL/APP 分区 + jump/reset 回调

    // 各设备装配结果
    bool enc_ok;
    bool rgb_ok;
    bool sense_ok;
    bool flash_ok;
    bool iap_ok;
    bool param_flash_ok;
} tDevBoard;

// 全局设备对象（装配完成后业务层直接使用）
extern tDevBoard g_dev;

// 装配并初始化全板设备：platform_init → 无参工厂 create → abs init。
// 返回 g_dev.enc_ok（编码器为本板关键设备）。
bool dev_board_init(void);

#endif // __DEV_BOARD_H
