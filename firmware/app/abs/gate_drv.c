// ============================================================
// gate_drv.c — 电机功率级业务对象（abs）
//
// 只做"状态维护 + 转发"：硬件动作经 ops + handle（装配时挂）直达驱动。
// ============================================================

#include "gate_drv.h"

bool gate_drv_init(tGateDrv *drv)
{
    if (!drv || !drv->ops || !drv->handle || !drv->ops->open)
        return false;

    drv->pwm_period = 0U;
    drv->dstate = DEV_OFFLINE;

    if (!drv->ops->open(drv->handle, &drv->pwm_period))
    {
        drv->dstate = DEV_RUN_ERROR;
        return false;
    }

    drv->dstate = DEV_ONLINE;
    return true;
}

void gate_drv_power_on(tGateDrv *drv, bool on)
{
    if (!drv || !drv->ops || !drv->ops->power)
        return;
    drv->ops->power(drv->handle, on);
    drv->dstate = on ? DEV_ONLINE : DEV_OFFLINE;
}

void gate_drv_enable(tGateDrv *drv, bool en)
{
    if (!drv || !drv->ops || !drv->ops->enable)
        return;
    drv->ops->enable(drv->handle, en);
    drv->dstate = en ? DEV_RUNNING : DEV_ONLINE;
}

void gate_drv_set_compare(tGateDrv *drv, uint16_t ticA, uint16_t ticB, uint16_t ticC)
{
    if (!drv || !drv->ops || !drv->ops->set_compare)
        return;
    drv->ops->set_compare(drv->handle, ticA, ticB, ticC);
}

void gate_drv_register_isrs(tGateDrv *drv, void (*sample_cb)(void), void (*ctrl_cb)(void))
{
    if (!drv || !drv->ops || !drv->ops->set_isr)
        return;
    drv->ops->set_isr(drv->handle, sample_cb, ctrl_cb);
}
