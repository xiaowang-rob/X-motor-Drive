#ifndef __DEV_BOARD_H
#define __DEV_BOARD_H

#include <stdbool.h>

#include "usr/abs/device.h"
#include "usr/abs/encoder.h"
#include "usr/abs/flash.h"
#include "usr/abs/led.h"
#include "usr/abs/sense.h"
#include "usr/abs/time.h"

// ============================================================
// dev_board.h — 组装层（usr/app）：板级设备装配的唯一入口
//
// 组装层是唯一同时 include hw 头 + drv 头 + abs 头的地方；
// 它把"板上资源(hw) + 芯片实现(drv)"绑定成实例、初始化 abs 业务对象，
// 并以全局对象 g_dev 暴露给业务层。
//
// 业务层只 include 本头（及 usr/abs），永远接触不到 hw/drv/厂商符号。
// ============================================================

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
