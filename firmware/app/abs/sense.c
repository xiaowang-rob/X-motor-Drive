// ============================================================
// sense.c — 采样业务对象实现
// ============================================================

#include "sense.h"

bool sense_init(tSense *s)
{
    if (!s || !s->ops || !s->handle)
        return false;
    s->dstate = DEV_OFFLINE;
    for (uint8_t i = 0U; i < 3U; i++)
    {
        s->cur_zero[i] = 0.0f;
        s->cur[i] = 0.0f;
    }
    s->zero_ready = false;
    s->vbus = 0.0f;
    s->temperature = 0.0f;

    // 启动采样前端
    if (!s->ops->open(s->handle))
        return false;

    s->dstate = DEV_ONLINE;
    return true;
}

void sense_set_sample_point(tSense *s, uint32_t tic)
{
    if (!s || !s->ops || !s->ops->set_sample_point)
        return;
    s->ops->set_sample_point(s->handle, tic);
}

void sense_update(tSense *s, bool motor_idle)
{
    if (!s || !s->ops || !s->ops->get_cur_raw)
        return;

    float cur_raw[3];
    if (!s->ops->get_cur_raw(s->handle, cur_raw))
        return;

    // ---- 三相电流 / 零点 跟随----
    if (motor_idle)
    {
        // 空闲：电流应为 0，用采样码累积零点（EMA）
        for (uint8_t i = 0U; i < 3U; i++)
        {
            if (!s->zero_ready)
                s->cur_zero[i] = cur_raw[i]; // 首次直接取，避免大跳
            else
                s->cur_zero[i] += (cur_raw[i] - s->cur_zero[i]) * SENSE_IDLE_K;
        }
        s->zero_ready = true;
    }
    else
    {
        if (!s->zero_ready)
        {
            for (uint8_t i = 0U; i < 3U; i++)
                s->cur[i] = 0.0f;

            s->dstate = DEV_RUN_ERROR;
            return;
        }
    }

    for (uint8_t i = 0U; i < 3U; i++)
        s->cur[i] = cur_raw[i] - s->cur_zero[i];

    s->ops->get_vt(&s->handle, &s->vbus, &s->temperature);
    s->dstate = DEV_RUNNING;
}
