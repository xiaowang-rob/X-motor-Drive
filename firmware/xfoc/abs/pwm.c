
#include "pwm.h"

bool pwm_init(tPwmDrv *drv, const tPwmDevHandle *handle) {
    if (!drv || !handle || !handle->ops || !handle->dev)
        return false;
    drv->handle = handle;

    if (!drv || !drv->handle || !drv->handle->ops)
        return false;

    return true;
}

void pwm_power_on(tPwmDrv *drv, bool on) {
    if (!drv || !drv->handle || !drv->handle->ops || !drv->handle->ops->power)
        return;
    drv->handle->ops->power(drv->handle->dev, on);
}

void pwm_enable(tPwmDrv *drv, bool en) {
    if (!drv || !drv->handle || !drv->handle->ops || !drv->handle->ops->enable)
        return;
    drv->handle->ops->enable(drv->handle->dev, en);
}

void pwm_set_compare(tPwmDrv *drv, uint16_t ticA, uint16_t ticB,
                     uint16_t ticC) {
    if (!drv || !drv->handle || !drv->handle->ops ||
        !drv->handle->ops->set_compare)
        return;
    drv->handle->ops->set_compare(drv->handle->dev, ticA, ticB, ticC);
}
