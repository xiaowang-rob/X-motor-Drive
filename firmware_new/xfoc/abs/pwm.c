// ============================================================
// gate_drv.c — 电机功率级业务对象（abs）
//
// 只做"状态维护 + 转发"：硬件动作经 ops + handle（装配时挂）直达驱动。
// ============================================================

#include "pwm.h"

bool gate_drv_init(tGateDrv *drv, const tGateDevHandle *handle) {
    if (!drv || !handle || !handle->ops || !handle->dev)
        return false;
    drv->handle = handle;

    if (!drv || !drv->handle || !drv->handle->ops || !drv->handle->ops->open)
        return false;

    drv->pwm_period = 0U;
    drv->dstate = DEV_OFFLINE;

    if (!drv->handle->ops->open(drv->handle->dev, &drv->pwm_period)) {
        drv->dstate = DEV_RUN_ERROR;
        return false;
    }

    drv->dstate = DEV_ONLINE;
    return true;
}

void gate_drv_power_on(tGateDrv *drv, bool on) {
    if (!drv || !drv->handle || !drv->handle->ops || !drv->handle->ops->power)
        return;
    drv->handle->ops->power(drv->handle->dev, on);
    drv->dstate = on ? DEV_ONLINE : DEV_OFFLINE;
}

void gate_drv_enable(tGateDrv *drv, bool en) {
    if (!drv || !drv->handle || !drv->handle->ops || !drv->handle->ops->enable)
        return;
    drv->handle->ops->enable(drv->handle->dev, en);
    drv->dstate = en ? DEV_RUNNING : DEV_ONLINE;
}

void gate_drv_set_compare(tGateDrv *drv, uint16_t ticA, uint16_t ticB, uint16_t ticC) {
    if (!drv || !drv->handle || !drv->handle->ops || !drv->handle->ops->set_compare)
        return;
    drv->handle->ops->set_compare(drv->handle->dev, ticA, ticB, ticC);
}
