#ifndef __ABS_DEVICE_H
#define __ABS_DEVICE_H

#include <stdint.h>  // uint8_t / uint16_t / uint32_t / intN_t / size_t
#include <stdbool.h> // bool
#include <stddef.h>  // NULL / size_t / ptrdiff_t
#include <string.h>  // memcpy / memset / memcmp

// ============================================================
// device.h — 跨层基础契约（drv / abs / 业务层共享）
//
// 约定：
//   - 本工程统一使用 <stdint.h> 的 uint8_t / uint16_t / uint32_t，
//     不使用 u8 / u16 / u32 之类的别名，避免类型来源不明。
//   - eDeviceStatus 是 drv 与 abs 之间唯一的"设备健康状态"词汇。
//     状态由**抽象实例**持有（业务视角），驱动层只返回单次操作的 bool 成败；
//     因此并非每个设备都需要状态 —— 纯数据流（采样）与纯输出（灯效）不加。
// ============================================================

typedef enum
{
    DEV_OFFLINE = 0, // 未初始化 / 离线
    DEV_ONLINE,      // 初始化成功，可工作，空闲
    DEV_RUN_ERROR,   // 通信 / 数据错误（可恢复，靠上层重试策略）
    DEV_RUNNING,     // 正在正常工作
    DEV_BUSY         // 正在忙（不可工作，需上层干预）
} eDeviceStatus;

#endif // __ABS_DEVICE_H
