#ifndef XDR_DFU_H
#define XDR_DFU_H

#include <stdbool.h>
#include <stdint.h>

/* 固件目标由启动程序提供。写入地址以目标分区起点为基准。
 * begin 失败时由目标清理其状态。finish 成功表示验证并提交完毕。 */
typedef struct {
    bool (*begin)(void *dev);
    bool (*write)(void *dev, uint32_t offset, const uint8_t *data, uint16_t len);
    bool (*finish)(void *dev, uint32_t image_size);
    void (*abort)(void *dev);
} tDfuOps;

typedef struct {
    const tDfuOps *ops;
    void *dev;
    uint32_t capacity;
    uint32_t poll_timeout_ms;
} tDfuDevHandle;

#endif
