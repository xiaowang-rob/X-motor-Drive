#ifndef __DEVICE_H
#define __DEVICE_H

#include <stdint.h>  // uint8_t / uint16_t / uint32_t / intN_t / size_t
#include <stdbool.h> // bool
#include <stddef.h>  // NULL / size_t / ptrdiff_t
#include <string.h>  // memcpy / memset / memcmp

// ============================================================
// device.h — 跨层基础契约（drv / abs / 业务层共享）
// ============================================================

typedef enum
{
    DEV_OFFLINE = 0, // 未初始化 / 离线
    DEV_ONLINE,      // 初始化成功，可工作，空闲
    DEV_RUN_ERROR,   // 通信 / 数据错误（可恢复，靠上层重试策略）
    DEV_RUNNING,     // 正在正常工作
    DEV_BUSY         // 正在忙（不可工作，需上层干预）
} eDeviceStatus;

#endif // __DEVICE_H
