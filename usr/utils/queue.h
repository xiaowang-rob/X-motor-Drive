#ifndef __QUEUE_H
#define __QUEUE_H

#include <stdbool.h>
#include <stdint.h>

// 队列状态枚举
typedef enum
{
    QUEUE_STATUS_OK = 0,
    QUEUE_STATUS_FULL,
    QUEUE_STATUS_EMPTY,
    QUEUE_STATUS_ERROR
} eQueueStatus;

// 静态队列结构体
typedef struct
{
    uint8_t *buffer;         // 数据缓冲区
    volatile uint16_t front; // 队首索引（读）
    volatile uint16_t rear;  // 队尾索引（写）
    volatile uint16_t count; // 当前元素个数
    uint16_t capacity;       // 容量（必须为2的幂，最大元素数）
    uint16_t mask;           // 容量掩码 = capacity - 1
    bool cover;              // 是否允许覆盖旧数据
} tStaticQueue;

// 函数声明
eQueueStatus queue_static_init(tStaticQueue *queue, uint8_t *buffer, uint16_t capacity);
void queue_clear(tStaticQueue *queue);
bool queue_is_empty(const tStaticQueue *queue);
bool queue_is_full(const tStaticQueue *queue);
uint16_t queue_count(const tStaticQueue *queue);
uint16_t queue_remaining(const tStaticQueue *queue);
eQueueStatus queue_static_enqueue(tStaticQueue *queue, const uint8_t *data);
eQueueStatus queue_static_dequeue(tStaticQueue *queue, uint8_t *data);

eQueueStatus queue_static_enqueue_bulk(tStaticQueue *queue, const uint8_t *data, uint16_t len);
eQueueStatus queue_static_dequeue_bulk(tStaticQueue *queue, uint8_t *data, uint16_t len);

eQueueStatus queue_static_peek(const tStaticQueue *queue, uint8_t *data);
eQueueStatus queue_static_peek_at(const tStaticQueue *queue, uint16_t index, uint8_t *data);

#endif // __QUEUE_H