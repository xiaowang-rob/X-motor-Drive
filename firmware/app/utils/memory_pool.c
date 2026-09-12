
#include "memory_pool.h"

// ==================== 初始化（传入外部静态数组） ====================
void mp_init(tMemPool *mp, uint8_t *buf, uint8_t data_size, uint8_t pool_block_count)
{
    mp->block_count = pool_block_count;
    mp->block_size = sizeof(tBlock) + data_size; // 每块大小 = 指针域 + 数据域

    // 将外部静态数组 buf 划分为 pool_block_count 个块，串成单向链表
    for (uint16_t i = 0; i < pool_block_count - 1; i++)
    {
        // 计算当前块和下一块的物理起始地址
        tBlock *current = (tBlock *)(buf + i * mp->block_size);
        tBlock *next = (tBlock *)(buf + (i + 1) * mp->block_size);
        current->next = next;
    }
    // 最后一块的 next 指向 NULL
    tBlock *last = (tBlock *)(buf + (pool_block_count - 1) * mp->block_size);
    last->next = NULL;

    // 空闲链表头指向第一块
    mp->free_list = (tBlock *)buf;
}

// ==================== 分配（中断安全） ====================
void *mp_alloc(tMemPool *mp)
{
    void *ptr = NULL;
    __disable_irq(); // 保护链表操作

    if (mp->free_list != NULL)
    {
        tBlock *block = mp->free_list;
        mp->free_list = block->next; // 头指针后移
        ptr = (void *)block->data;   // 返回数据区首地址（紧跟在 next 后面）
    }

    __enable_irq();
    return ptr;
}

// ==================== 释放（中断安全） ====================
void mp_free(tMemPool *mp, void *ptr)
{
    if (ptr == NULL)
        return;

    // 由数据区指针反算 tBlock 首地址：
    // 因为 data 紧跟在 next 之后，所以块首地址 = 数据区地址 - sizeof(tBlock*)
    tBlock *block = (tBlock *)((uint8_t *)ptr - sizeof(tBlock *));

    __disable_irq();
    // 头插法归还到空闲链表
    block->next = mp->free_list;
    mp->free_list = block;
    __enable_irq();
}