#ifndef __BUS_COM_H
#define __BUS_COM_H

#include "device.h"
#include "queue.h"
#include "memory_pool.h"

// ============================================================
// bus_com.h — 总线式通信业务对象（abs）
//
// 职责：内存池 + 帧队列 + 设备状态。驱动在中断里回调注册进来的 cb
// （回传 ctx = 本实例），本层把帧实体从内存池取出、填好后把"指针"入队，
// 主线程用 bus_process_frame 提取；驱动不持有内存池与队列。
//
// 缓冲由**调用方提供**（无文件级静态单例）→ 同一份代码可挂多路总线。
// 接口形态：ops + handle（装配层在定义 tBusDriver 对象时挂上）。
// ============================================================

// bus 类数据帧（总线式：ID + 数据）
typedef struct
{
    uint32_t id;      // 总线 ID（CAN：标准帧 11 位 / 扩展帧 29 位）
    uint8_t data_len; // 有效字节数（≤ 8）
    uint8_t data[8];  // 载荷（CAN 经典帧上限）
} tBus_Frame;

// 缓冲描述（调用方提供）
typedef struct
{
    uint8_t *mp_buf;       // 内存池缓冲：每块 = 一个 tBus_Frame
    uint16_t mp_block_num; // 块数（≤ 255）
    uint8_t *queue_buf;    // 帧队列缓冲（按字节计，存帧实体地址）
    uint16_t queue_bytes;  // 队列容量字节数（须为 2 的幂，且 ≥ 4× 期望帧数）
} tBusBuffer;

// 收帧回调：驱动在中断上下文调用；ctx 为注册时传入的实例指针
typedef void (*bus_rx_frame_cb)(void *ctx, uint32_t id, const uint8_t *data, uint8_t len);

// 驱动接口（板级实现）；handle 为驱动实例
typedef struct
{
    bool (*open)(void *handle, uint32_t std_id);                       // 配置过滤器并启动总线
    bool (*send)(void *handle, uint32_t id, const uint8_t *data,
                 uint16_t len);                                        // 发送一帧（忙则 false）
    void (*set_rx_cb)(void *handle, bus_rx_frame_cb cb, void *ctx);    // 注册收帧回调
} tBusOps;

typedef struct
{
    const tBusOps *ops; // 驱动 ops（装配时挂）
    void *handle;       // 驱动实例（装配时挂）

    uint32_t device_id;       // 设备 id
    eDeviceStatus dstate;     // 设备状态
    eDeviceStatus tstate;     // 发送状态
    eDeviceStatus rstate;     // 接收状态
    tMemPool mem_pool;        // 内存池（存放帧实体，缓冲由调用方提供）
    tStaticQueue frame_queue; // 队列（存放帧实体地址，缓冲由调用方提供）
} tBusDriver;

// 启动对象：ops/handle 须已由装配层挂好；初始化内存池 / 队列 / 收帧回调
bool bus_init(tBusDriver *bus, const tBusBuffer *buf);

// 以设备 ID 启动总线（内部调用 ops->open）
bool bus_start(tBusDriver *bus, uint32_t device_id);

// 发送一帧
bool bus_send(tBusDriver *bus, const tBus_Frame *frame);

// 持续从队列中提取数据帧并返回；没有数据帧则返回 NULL。
// 注意：返回的帧实体来自内存池，调用方用毕必须 mp_free 归还。
tBus_Frame *bus_process_frame(tBusDriver *bus);

#endif // __BUS_COM_H
