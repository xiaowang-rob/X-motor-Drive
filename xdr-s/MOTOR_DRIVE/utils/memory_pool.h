#ifndef __MEMORY_POOL_H
#define __MEMORY_POOL_H

#include <stdint.h>
#include <string.h>

#define CREATE_MEM_POOL_BUF(buf, data_size, pool_block_count) \
    static uint8_t buf[pool_block_count * (sizeof(tBlock *) + data_size)];

// ==================== 内存块定义 ====================
// 核心思想：链表指针 (next) 放在每块的最前面，用户数据紧跟在指针后面
typedef struct tBlock
{
    struct tBlock *next; // 指向下一个空闲块（占用 4/8 字节）

    uint8_t data[]; // 柔性数组，实际不占 sizeof，但代表紧随其后的地址
} tBlock;

// ==================== 内存池主体 ====================
typedef struct
{
    tBlock *free_list;   // 空闲链表头
    uint16_t block_size; // 每块总字节数 = sizeof(tBlock) + data_size
    uint8_t block_count; // 总块数
} tMemPool;

void mp_init(tMemPool *mp, uint8_t *buf, uint8_t data_size, uint8_t pool_block_count);
void *mp_alloc(tMemPool *mp);
void mp_free(tMemPool *mp, void *ptr);

#endif