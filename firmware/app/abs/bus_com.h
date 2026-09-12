#ifndef __BUS_COM_H
#define __BUS_COM_H

#include "device.h"
#include "queue.h"
#include "memory_pool.h"

// 对总线通信的抽象 通常是设备id+数据帧的形式出现

// 接收处理方式 ： 接收数据进内存池 然后将内存池地址进队列 通过主线程提取处理
// 发送处理方式 ： 通过主线程发送

// bus类数据帧-总线式
typedef struct
{
    uint8_t id;
    uint8_t data_len;
    uint8_t data[8];
} tBus_Frame;

// bus 收帧回调
typedef void (*bus_rx_frame_cb)(uint32_t id, const uint8_t *data, uint8_t len);

typedef struct
{
    bool (*init)(uint32_t std_id);
    bool (*send)(uint8_t id, uint8_t *data, uint16_t length);
    void (*register_callback)(bus_rx_frame_cb callback);
} tBusDriverOps;

typedef struct
{
    const tBusDriverOps *ops; // 操作函数指针
    uint32_t device_id;       // 设备id
    eDeviceStatus dstate;     // 设备状态
    eDeviceStatus tstate;     // 发送状态
    eDeviceStatus rstate;     // 接收状态
    tMemPool mem_pool;        // 内存池
    tStaticQueue frame_queue; // 数据帧队列
} tBusDriver;

// 初始化总线驱动 和 内存池 和 队列
bool bus_init(tBusDriver *bus, tBusDriverOps *ops);
bool bus_start(tBusDriver *bus, uint32_t device_id);
bool bus_send(tBusDriver *bus, tBus_Frame *frame);

// 这个会持续从队列中提取数据帧并返回 没有数据帧则返回NULL
tBus_Frame *bus_process_frame(tBusDriver *bus);

#endif
