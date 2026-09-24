#ifndef __FIELD_H
#define __FIELD_H

#include <stdint.h>
#include <stdbool.h>

// 只保留协议实际用到的 4 种（需要时再扩）
typedef enum
{
    FLD_U8 = 0,
    FLD_U32,
    FLD_F32,
    FLD_BOOL,
} eFieldType;

typedef struct
{
    void *ptr;    // 字段地址（&结构体.字段）
    uint8_t type; // eFieldType
    float def;    // 默认值
} tField;

// ---- 引擎 ----
uint8_t field_read_raw(const tField *f, uint8_t *dst);     // 读原始字节，返回长度
bool field_write_raw(const tField *f, const uint8_t *src); // 写原始字节

#endif // __FIELD_H
