#ifndef __PWM_H
#define __PWM_H

#include "device_status.h"

typedef struct {
    uint16_t pwm_arr;
    uint16_t ticTs;
    uint16_t ticTn;
    uint16_t ticTd;
} tPwmCfg;
// 驱动接口（板级实现）；handle 为驱动实例
typedef struct {
    void (*open)(void *handle, tPwmCfg *cfg); // PWM 配置初始化
    void (*power)(void *handle, bool on);     // 12V 功率级电源开关
    void (*enable)(void *handle, bool en);    // PWM 输出使能 / 关断
    void (*set_compare)(void *handle, uint16_t a, uint16_t b,
                        uint16_t c); // 三相占空比（热路径）
} tPwmOps;

// BSP 导出的类型化句柄：操作表 + 驱动实例。
typedef struct {
    const tPwmOps *ops;
    void *dev;
} tPwmDevHandle;

typedef struct {
    const tPwmDevHandle *handle; // 驱动句柄，由 init 注入
    tPwmCfg cfg;
} tPwmDrv;

extern tPwmDrv g_pwm;
// 启动对象：显式传入 BSP 导出的类型化句柄；缓冲由调用方提供。
bool pwm_init(tPwmDrv *drv, const tPwmDevHandle *handle);

void pwm_power_on(tPwmDrv *drv, bool on);
void pwm_enable(tPwmDrv *drv, bool en);
void pwm_set_compare(tPwmDrv *drv, uint16_t ticA, uint16_t ticB, uint16_t ticC);

#endif // __PWM_H
