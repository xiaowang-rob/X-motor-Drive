#include "queue.h"

#include <stddef.h> // NULL
#include <string.h> // memcpy

//  临界区开关
#define QUEUE_ENABLE_CRITICAL 0 // 0-关闭（单线程），1-开启（多任务/中断）

#if QUEUE_ENABLE_CRITICAL
// 若使用 CMSIS 或 Cortex-M
#define QUEUE_ENTER_CRITICAL() __disable_irq()
#define QUEUE_EXIT_CRITICAL() __enable_irq()

// 若使用 FreeRTOS，可替换为：
// #define QUEUE_ENTER_CRITICAL()   taskENTER_CRITICAL()
// #define QUEUE_EXIT_CRITICAL()    taskEXIT_CRITICAL()
#else
#define QUEUE_ENTER_CRITICAL() ((void)0)
#define QUEUE_EXIT_CRITICAL() ((void)0)
#endif

//  内部辅助函数
static inline bool isQueueNull(const tStaticQueue *queue)
{
    return (queue == NULL);
}

//  初始化
eQueueStatus queue_static_init(tStaticQueue *queue, uint8_t *buffer, uint16_t capacity)
{
    if (isQueueNull(queue) || buffer == NULL || capacity == 0)
        return QUEUE_STATUS_ERROR;

    // 检查 capacity 是否为 2 的幂
    if ((capacity & (capacity - 1)) != 0)
        return QUEUE_STATUS_ERROR;

    queue->buffer = buffer;
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
    queue->capacity = capacity;
    queue->mask = capacity - 1;
    queue->cover = false; // 默认不允许覆盖，用户可按需修改

    return QUEUE_STATUS_OK;
}

//  清空
void queue_clear(tStaticQueue *queue)
{
    if (isQueueNull(queue))
        return;
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
    // 注意：cover 状态不变，保留用户设置
}

//  查询状态
bool queue_is_empty(const tStaticQueue *queue)
{
    if (isQueueNull(queue))
        return true;
    return (queue->count == 0);
}

bool queue_is_full(const tStaticQueue *queue)
{
    if (isQueueNull(queue))
        return true;
    return (queue->count == queue->capacity);
}

uint16_t queue_count(const tStaticQueue *queue)
{
    if (isQueueNull(queue))
        return 0;
    return queue->count;
}

uint16_t queue_remaining(const tStaticQueue *queue)
{
    if (isQueueNull(queue))
        return 0;
    return (queue->capacity - queue->count);
}

//  单字节入队
eQueueStatus queue_static_enqueue(tStaticQueue *queue, const uint8_t *data)
{
    if (isQueueNull(queue) || data == NULL)
        return QUEUE_STATUS_ERROR;

    // 快速判断：如果队列未满，直接入队
    if (queue->count < queue->capacity)
    {
        QUEUE_ENTER_CRITICAL();
        // 再次检查，防止竞争条件
        if (queue->count < queue->capacity)
        {
            queue->buffer[queue->rear] = *data;
            queue->rear = (queue->rear + 1) & queue->mask;
            queue->count++;
            QUEUE_EXIT_CRITICAL();
            return QUEUE_STATUS_OK;
        }
        QUEUE_EXIT_CRITICAL();
    }

    // 队列已满，检查是否允许覆盖
    if (queue->cover)
    {
        QUEUE_ENTER_CRITICAL();
        // 再次确认满状态
        if (queue->count == queue->capacity)
        {
            // 覆盖模式：丢弃队首数据（front 后移）
            queue->front = (queue->front + 1) & queue->mask;
            // 写入新数据到当前 rear 位置
            queue->buffer[queue->rear] = *data;
            // rear 后移（覆盖后 rear 仍指向下一个空位，但满时保持环状）
            queue->rear = (queue->rear + 1) & queue->mask;
            // count 保持不变（仍为 capacity）
            QUEUE_EXIT_CRITICAL();
            return QUEUE_STATUS_OK;
        }
        else
        {
            // 这里理论上不会发生（因为外层已判断满），但保留安全处理
            QUEUE_EXIT_CRITICAL();
            // 实际未满，可尝试再次入队（但递归调用可能死循环，这里简单返回错误）
            return QUEUE_STATUS_ERROR;
        }
    }
    else
    {
        // 不允许覆盖，返回满错误
        return QUEUE_STATUS_FULL;
    }
}

//  出队
eQueueStatus queue_static_dequeue(tStaticQueue *queue, uint8_t *data)
{
    if (isQueueNull(queue))
        return QUEUE_STATUS_ERROR;

    QUEUE_ENTER_CRITICAL();

    if (queue->count == 0)
    {
        QUEUE_EXIT_CRITICAL();
        return QUEUE_STATUS_EMPTY;
    }

    if (data != NULL)
        *data = queue->buffer[queue->front];

    queue->front = (queue->front + 1) & queue->mask;
    queue->count--;

    QUEUE_EXIT_CRITICAL();
    return QUEUE_STATUS_OK;
}
//  多字节入队
eQueueStatus queue_static_enqueue_bulk(tStaticQueue *queue, const uint8_t *data, uint16_t len)
{
    if (isQueueNull(queue) || data == NULL)
        return QUEUE_STATUS_ERROR;

    if (len == 0)
        return QUEUE_STATUS_OK;

    QUEUE_ENTER_CRITICAL();

    uint16_t free = queue->capacity - queue->count;

    // 情况1：空间足够，直接写入
    if (len <= free)
    {
        // 拷贝 len 字节到环形缓冲区
        uint16_t rear = queue->rear;
        uint16_t first = queue->capacity - rear; // 从 rear 到缓冲区末尾的连续空间
        if (first >= len)
        {
            memcpy(&queue->buffer[rear], data, len);
        }
        else
        {
            memcpy(&queue->buffer[rear], data, first);
            memcpy(queue->buffer, data + first, len - first);
        }
        queue->rear = (rear + len) & queue->mask;
        queue->count += len;
        QUEUE_EXIT_CRITICAL();
        return QUEUE_STATUS_OK;
    }

    // 空间不足，检查是否允许覆盖
    if (!queue->cover)
    {
        QUEUE_EXIT_CRITICAL();
        return QUEUE_STATUS_FULL;
    }

    // ---------- 覆盖模式 ----------
    // 如果 len >= capacity，则只保留最后 capacity 个字节，重置整个队列
    if (len >= queue->capacity)
    {
        const uint8_t *src = data + (len - queue->capacity);
        memcpy(queue->buffer, src, queue->capacity);
        queue->front = 0;
        queue->rear = 0;
        queue->count = queue->capacity;
        QUEUE_EXIT_CRITICAL();
        return QUEUE_STATUS_OK;
    }

    // 此时 len < capacity 且 len > free，需要覆盖 (len - free) 个旧数据
    uint16_t overwrite = len - free;
    // 丢弃队首的 overwrite 个字节
    queue->front = (queue->front + overwrite) & queue->mask;
    queue->count -= overwrite; // 现在 count = capacity - len

    // 现在有足够的空闲空间（正好 len 个），写入新数据
    uint16_t rear = queue->rear;
    uint16_t first = queue->capacity - rear;
    if (first >= len)
    {
        memcpy(&queue->buffer[rear], data, len);
    }
    else
    {
        memcpy(&queue->buffer[rear], data, first);
        memcpy(queue->buffer, data + first, len - first);
    }
    queue->rear = (rear + len) & queue->mask;
    queue->count += len; // 变为 capacity

    QUEUE_EXIT_CRITICAL();
    return QUEUE_STATUS_OK;
}

//  多字节出队
eQueueStatus queue_static_dequeue_bulk(tStaticQueue *queue, uint8_t *data, uint16_t len)
{
    if (isQueueNull(queue) || data == NULL)
        return QUEUE_STATUS_ERROR;

    if (len == 0)
        return QUEUE_STATUS_OK;

    QUEUE_ENTER_CRITICAL();

    if (queue->count < len)
    {
        QUEUE_EXIT_CRITICAL();
        return QUEUE_STATUS_EMPTY;
    }

    // 拷贝 len 字节从环形缓冲区到 data
    uint16_t front = queue->front;
    uint16_t first = queue->capacity - front;
    if (first >= len)
    {
        memcpy(data, &queue->buffer[front], len);
    }
    else
    {
        memcpy(data, &queue->buffer[front], first);
        memcpy(data + first, queue->buffer, len - first);
    }

    queue->front = (front + len) & queue->mask;
    queue->count -= len;

    QUEUE_EXIT_CRITICAL();
    return QUEUE_STATUS_OK;
}

//  查看队首
eQueueStatus queue_static_peek(const tStaticQueue *queue, uint8_t *data)
{
    if (isQueueNull(queue) || data == NULL)
        return QUEUE_STATUS_ERROR;

    if (queue->count == 0)
        return QUEUE_STATUS_EMPTY;

    *data = queue->buffer[queue->front];
    return QUEUE_STATUS_OK;
}

//  查看指定索引
eQueueStatus queue_static_peek_at(const tStaticQueue *queue, uint16_t index, uint8_t *data)
{
    if (isQueueNull(queue) || data == NULL)
        return QUEUE_STATUS_ERROR;

    if (queue->count == 0)
        return QUEUE_STATUS_EMPTY;

    if (index >= queue->count)
        return QUEUE_STATUS_ERROR;

    uint16_t actual_index = (queue->front + index) & queue->mask;
    *data = queue->buffer[actual_index];
    return QUEUE_STATUS_OK;
}