#ifndef __GATE_DRV_H
#define __GATE_DRV_H

#include "device.h"

typedef struct
{
    // 获取pwm配置
    void (*get_pwm_config)(uint32_t *pwm_period);
    void (*power_ctrl)(bool on);
    void (*start)(void);
    void (*stop)(void);
    void (*set_compare)(uint16_t ticA, uint16_t ticB, uint16_t ticC);

} tGateDrvOps;

typedef struct
{
    tGateDrvOps *ops;

    uint16_t pwm_period;
    eDeviceStatus dstate;
    // 其他私有数据
} tGateDrv;

void gate_drv_init(tGateDrv *drv, tGateDrvOps *ops);
uint16_t gate_drv_get_pwm_period(tGateDrv *drv) { return drv->pwm_period; };
void gate_drv_power_on(tGateDrv *drv, bool on);
void gate_drv_start(tGateDrv *drv);
void gate_drv_stop(tGateDrv *drv);
void gate_drv_set_compare(tGateDrv *drv, uint16_t ticA, uint16_t ticB, uint16_t ticC);
eDeviceStatus gate_drv_get_status(tGateDrv *drv) { return drv->dstate; };

#endif