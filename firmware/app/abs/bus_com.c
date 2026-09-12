#include "bus_com.h"

// 内存池 0
#define MP0_BLOCK_SIZE sizeof(tBus_Frame) // 每个块数据大小 byte 一定大于等于总线消息帧
#define MP0_BLOCK_NUM 16                  // 存储最多消息帧数量

CREATE_MEM_POOL_BUF(bus_mp_buf0, MP0_BLOCK_SIZE, MP0_BLOCK_NUM)

// 内存池列表 暂时只创建一个内存池 暂时只有一个总线
// TODO:可以修改为多个内存池 初始化分配

// 创建数据帧队列缓存区-存储bus消息内存池指针 这里对应的也分配一个队列
static uint8_t bus_frame_buf0[MP0_BLOCK_NUM];

void bus_rx_frame_cb(tBusDriver *bus, uint32_t id, const uint8_t *data, uint8_t len)
{
    uint8_t *rx_data = (uint8_t *)mp_alloc(&bus->mem_pool);
    if (!rx_data)
        return;
    tBus_Frame *frame = (tBus_Frame *)rx_data; // 将内存池分配的内存转换为tCAN_Frame结构体指针
    frame->id = id;
    frame->data_len = len;
    memcpy(frame->data, data, len);

    // 将消息内存地址放入队列 单字节入队即可
    eQueueStatus qs = queue_static_enqueue(&bus->frame_queue, rx_data);
    if (QUEUE_STATUS_ERROR == qs)
    {
        bus->rstate = DEV_RUN_ERROR; // 如果内存分配失败 则总线状态为错误
        return;
    }
    else if (QUEUE_STATUS_OK != qs)
    {
        bus->rstate = DEV_BUSY; // 满队列则总线状态为忙
        return;
    }
}

// 初始化内存池 和 队列
bool bus_init(tBusDriver *bus, tBusDriverOps *ops)
{
    if (bus == NULL || ops == NULL)
        return false;

    bus->ops = ops;

    bus->dstate = DEV_OFFLINE;

    // 创建内存池
    mp_init(&bus->mem_pool, bus_mp_buf0, MP0_BLOCK_SIZE, MP0_BLOCK_NUM);

    bus->frame_queue.cover = false; // 不允许覆盖旧数据 阻塞式接收
    if (QUEUE_STATUS_OK != queue_static_init(&bus->frame_queue, bus_frame_buf0, MP0_BLOCK_NUM))
        return false;

    // 注册回调函数
    bus->ops->register_callback(bus_rx_frame_cb);

    return true;
}

bool bus_start(tBusDriver *bus, uint32_t device_id)
{
    if (!bus)
        return false;
    bus->device_id = device_id;
    if (!bus->ops->init(device_id)) // 初始化总线驱动
        return false;
    bus->dstate = DEV_ONLINE;
    bus->rstate = DEV_ONLINE; // 初始化总线状态为在线
    bus->tstate = DEV_ONLINE; // 初始化总线状态为在线
}

bool bus_send(tBusDriver *bus, tBus_Frame *frame)
{
    if (bus == NULL || frame == NULL)
        return false;

    if (!bus->ops->send(frame->id, frame->data, frame->data_len)) // 发送总线消息
    {
        bus->tstate = DEV_BUSY; // 如果发送失败 则一般总线状态为忙
        return false;
    }

    bus->tstate = DEV_RUNNING;
    return true;
}

// 这个会持续从队列中提取数据帧并返回 没有数据帧则返回NULL
tBus_Frame *bus_process_frame(tBusDriver *bus)
{
    if (bus == NULL)
        return NULL;

    // 检查队列是否为空
    if (queue_static_is_empty(&bus->frame_queue))
        return NULL;

    // 从队列中取出数据帧
    tBus_Frame *frame;
    queue_static_dequeue(&bus->frame_queue, (uint8_t *)frame);
    if (frame == NULL)
        return NULL;
    return frame; // 返回数据帧
}
