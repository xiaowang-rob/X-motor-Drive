// ============================================================
// sample.c — 采样业务对象实现
// ============================================================

#include "sample.h"

bool sample_init(tSense *s, const tSampleDevHandle *handle) {
    if (!s || !handle || !handle->ops || !handle->dev)
        return false;
    s->handle = handle;

    if (!handle->ops->open || !handle->ops->get_cur_raw || !handle->ops->get_vt)
        return false;
    s->dstate = DEV_OFFLINE;
    for (uint8_t i = 0U; i < 3U; i++) {
        s->cur_zero[i] = 0.0f;
        s->cur[i] = 0.0f;
    }
    s->zero_ready = false;
    s->vbus = 0.0f;
    s->temperature = 0;

    // 启动采样前端
    if (!s->handle->ops->open(s->handle->dev))
        return false;

    do { // 采样前端启动后，等待 VBUS 上电
        if (!handle->ops->get_vt(handle->dev, &s->vbus, &s->temperature))
            return false;
    } while (s->vbus < 0.1f);

    s->dstate = DEV_ONLINE;
    return true;
}

void sample_set_sample_point(tSense *s, uint32_t tic) {
    if (!s || !s->handle || !s->handle->ops || !s->handle->ops->set_sample_point)
        return;
    s->handle->ops->set_sample_point(s->handle->dev, tic);
}

void sample_update(tSense *s, bool motor_idle) {
    if (!s || !s->handle || !s->handle->ops || !s->handle->ops->get_cur_raw)
        return;

    float cur_raw[3];
    if (!s->handle->ops->get_cur_raw(s->handle->dev, cur_raw))
        return;

    // ---- 三相电流 / 零点 跟随----
    if (motor_idle) {
        // 空闲：电流应为 0，用采样码累积零点（EMA）
        for (uint8_t i = 0U; i < 3U; i++) {
            if (!s->zero_ready)
                s->cur_zero[i] = cur_raw[i]; // 首次直接取，避免大跳
            else
                s->cur_zero[i] += (cur_raw[i] - s->cur_zero[i]) * SENSE_IDLE_K;
        }
        s->zero_ready = true;
    }
    else {
        if (!s->zero_ready) {
            for (uint8_t i = 0U; i < 3U; i++)
                s->cur[i] = 0.0f;

            s->dstate = DEV_RUN_ERROR;
            return;
        }
    }

    for (uint8_t i = 0U; i < 3U; i++)
        s->cur[i] = cur_raw[i] - s->cur_zero[i];

    s->handle->ops->get_vt(s->handle->dev, &s->vbus, &s->temperature);
    s->dstate = DEV_RUNNING;
}
