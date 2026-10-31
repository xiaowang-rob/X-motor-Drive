
#include "memory_pool.h"

// 临界区：保存并恢复 PRIMASK —— 避免在中断上下文里被误"开"全局中断
// （原来直接 __enable_irq() 会无条件开中断，在 ISR 中调用即出错）
#if defined(__ARM_ARCH)
#include "cmsis_compiler.h"
#define MP_ENTER()                    \
    uint32_t mp_primask = __get_PRIMASK(); \
    __disable_irq()
#define MP_EXIT()                    \
    do                               \
    {                                \
        if (mp_primask == 0U)        \
            __enable_irq();          \
    } while (0)
#else
#define MP_ENTER() ((void)0)
#define MP_EXIT() ((void)0)
#endif

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
    MP_ENTER();

    if (mp->free_list != NULL)
    {
        tBlock *block = mp->free_list;
        mp->free_list = block->next; // 头指针后移
        ptr = (void *)block->data;   // 返回数据区首地址（紧跟在 next 后面）
    }

    MP_EXIT();
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

    MP_ENTER();
    // 头插法归还到空闲链表
    block->next = mp->free_list;
    mp->free_list = block;
    MP_EXIT();
}