#include "bus_com.h"

#include <string.h>

// ============================================================
// bus_com.c — 总线式通信业务对象（abs，纯逻辑）
//
// 职责：把驱动送来的原始帧放进"内存池 + 队列"，供主线程提取。
// 驱动只负责在中断里回调注册进来的 cb（回传 ctx = 本实例）。
// 硬件动作经 ops + handle（装配时挂）直达驱动。
// 内存池 / 队列缓冲由调用方提供（见 tBusBuffer），本文件无静态缓冲。
// ============================================================

// 收帧回调（中断上下文）：ctx 为 bus_init 时绑定的 tBusDriver 实例
static void bus_on_rx_frame(void *ctx, uint32_t id, const uint8_t *data, uint8_t len)
{
    tBusDriver *bus = (tBusDriver *)ctx;
    if (!bus || !data)
        return;

    tBus_Frame *frame = (tBus_Frame *)mp_alloc(&bus->mem_pool);
    if (!frame)
    {
        bus->rstate = DEV_RUN_ERROR; // 内存池耗尽
        return;
    }

    frame->id = id;
    frame->data_len = (len > sizeof(frame->data)) ? (uint8_t)sizeof(frame->data) : len;
    memcpy(frame->data, data, frame->data_len);

    // 入队的是"帧实体地址"（按 bulk 写入 sizeof(指针) 字节）
    tBus_Frame *slot = frame;
    eQueueStatus qs = queue_static_enqueue_bulk(&bus->frame_queue,
                                                (const uint8_t *)&slot, sizeof(slot));
    if (qs == QUEUE_STATUS_OK)
    {
        bus->rstate = DEV_RUNNING;
    }
    else
    {
        mp_free(&bus->mem_pool, frame); // 入队失败：立即回收块，避免泄漏
        bus->rstate = DEV_BUSY;
    }
}

bool bus_init(tBusDriver *bus, const tBusBuffer *buf)
{
    if (!bus || !buf || !bus->ops || !bus->handle)
        return false;
    if (!buf->mp_buf || buf->mp_block_num == 0U || !buf->queue_buf || buf->queue_bytes == 0U)
        return false;

    // 只初始化业务字段；ops / handle 是装配结果，不在本层改动
    bus->device_id = 0U;
    bus->dstate = DEV_OFFLINE;
    bus->tstate = DEV_OFFLINE;
    bus->rstate = DEV_OFFLINE;

    mp_init(&bus->mem_pool, buf->mp_buf, sizeof(tBus_Frame), (uint8_t)buf->mp_block_num);

    bus->frame_queue.cover = false; // 不允许覆盖旧数据：满则丢弃新帧并置忙
    if (QUEUE_STATUS_OK != queue_static_init(&bus->frame_queue, buf->queue_buf, buf->queue_bytes))
        return false;

    // 注册回调并把本实例作为 ctx 回传
    if (bus->ops->set_rx_cb)
        bus->ops->set_rx_cb(bus->handle, bus_on_rx_frame, bus);
    return true;
}

bool bus_start(tBusDriver *bus, uint32_t device_id)
{
    if (!bus || !bus->ops || !bus->ops->open)
        return false;

    bus->device_id = device_id;
    if (!bus->ops->open(bus->handle, device_id))
    {
        bus->dstate = DEV_RUN_ERROR;
        return false;
    }

    bus->dstate = DEV_ONLINE;
    bus->rstate = DEV_ONLINE;
    bus->tstate = DEV_ONLINE;
    return true;
}

bool bus_send(tBusDriver *bus, const tBus_Frame *frame)
{
    if (!bus || !frame || !bus->ops || !bus->ops->send)
        return false;

    if (!bus->ops->send(bus->handle, frame->id, frame->data, frame->data_len))
    {
        bus->tstate = DEV_BUSY; // 发送失败一般因邮箱忙
        return false;
    }

    bus->tstate = DEV_RUNNING;
    return true;
}

tBus_Frame *bus_process_frame(tBusDriver *bus)
{
    if (!bus)
        return NULL;

    tBus_Frame *slot = NULL;
    if (QUEUE_STATUS_OK != queue_static_dequeue_bulk(&bus->frame_queue,
                                                     (uint8_t *)&slot, sizeof(slot)))
        return NULL;
    return slot;
}
