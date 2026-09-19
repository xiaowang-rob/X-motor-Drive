#ifndef XDR_APP_ABS_BUS_COM_H
#define XDR_APP_ABS_BUS_COM_H

#include "device.h"
#include "queue.h"
#include "memory_pool.h"

// ============================================================
// bus_com.h — 总线式通信业务对象（abs）
//
// 职责：内存池 + 帧队列 + 设备状态。板级在中断里回调注册进来的 cb
// （回传 ctx = 本实例），本层把帧实体从内存池取出、填好后把"指针"入队，
// 主线程用 bus_process_frame 提取；板级不持有内存池与队列。
//
// 缓冲由**调用方提供**（无文件级静态单例）→ 同一份代码可挂多路总线。
//
// 编译期绑定：硬件动作经板级钩子（bus_com_board.h）直接调用，
// 无 ops 表、无 void* 句柄；实例由 eBusPort 区分。
// ============================================================

// bus 类数据帧（总线式：ID + 数据）
typedef struct
{
    uint32_t id;      // 总线 ID（CAN：标准帧 11 位 / 扩展帧 29 位）
    uint8_t data_len; // 有效字节数（≤ 8）
    uint8_t data[8];  // 载荷（CAN 经典帧上限）
} tBus_Frame;

// 板级总线实例标识（编译期绑定；见 board_bus.c 的分派）
typedef enum
{
    BUS_PORT_CAN = 0, // CAN
    BUS_PORT_NUM
} eBusPort;

// 缓冲描述（调用方提供）
typedef struct
{
    uint8_t *mp_buf;       // 内存池缓冲：每块 = 一个 tBus_Frame
    uint16_t mp_block_num; // 块数（≤ 255）
    uint8_t *queue_buf;    // 帧队列缓冲（按字节计，存帧实体地址）
    uint16_t queue_bytes;  // 队列容量字节数（须为 2 的幂，且 ≥ 4× 期望帧数）
} tBusBuffer;

// 收帧回调：板级在中断上下文调用；ctx 为注册时传入的实例指针
typedef void (*bus_rx_frame_cb)(void *ctx, uint32_t id, const uint8_t *data, uint8_t len);

typedef struct
{
    eBusPort port;         // 板级实例
    uint32_t device_id;    // 设备 id
    eDeviceStatus dstate;  // 设备状态
    eDeviceStatus tstate;  // 发送状态
    eDeviceStatus rstate;  // 接收状态
    tMemPool mem_pool;     // 内存池（存放帧实体，缓冲由调用方提供）
    tStaticQueue frame_queue; // 队列（存放帧实体地址，缓冲由调用方提供）
} tBusDriver;

// 绑定板级实例、初始化内存池 / 队列 / 收帧回调（缓冲由调用方提供）
bool bus_init(tBusDriver *bus, eBusPort port, const tBusBuffer *buf);

// 以设备 ID 启动总线（内部调用板级钩子 bus_board_open）
bool bus_start(tBusDriver *bus, uint32_t device_id);

// 发送一帧
bool bus_send(tBusDriver *bus, const tBus_Frame *frame);

// 持续从队列中提取数据帧并返回；没有数据帧则返回 NULL。
// 注意：返回的帧实体来自内存池，调用方用毕必须 mp_free 归还。
tBus_Frame *bus_process_frame(tBusDriver *bus);

#endif // XDR_APP_ABS_BUS_COM_H
