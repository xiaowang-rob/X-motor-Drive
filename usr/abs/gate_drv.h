#ifndef __GATE_DRV_H
#define __GATE_DRV_H

#include "device.h"

typedef void *GateDrvHandle;

typedef struct
{
    void (*get_pwm_config)(GateDrvHandle handle, uint32_t *pwm_period, uint32_t *pwm_duty);
    void (*power_ctrl)(GateDrvHandle handle, bool on);
    void (*start)(GateDrvHandle handle);
    void (*stop)(GateDrvHandle handle);
    void (*set_compare)(GateDrvHandle handle, uint16_t ticA, uint16_t ticB, uint16_t ticC);

} tGateDrvOps;

#endif