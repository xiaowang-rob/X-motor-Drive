#ifndef __PARAMETERS_H
#define __PARAMETERS_H

#include "parameter_list.h"
#include "flash.h"
#include <stdint.h>
#include <stdbool.h>

tParameter g_param;

typedef struct
{
    void *ptr;    // 字段地址（&结构体.字段）
    uint8_t type; // eFieldType
    float def;    // 默认值
} tField;

// ---- 读写 / 持久化 ----
// 写入一个参数（value 为该参数的原始字节；id 越界 = "应用并保存" 的协议约定）
void param_set(eParameter para, uint8_t *value);

// 读取一个参数（value 收原始字节，len 出长度；id 越界时 len=0）
void param_get(eParameter para, uint8_t *value, uint8_t *len);

bool param_init(tFlash *flash); // 初始化存储单元、读取 、校验
bool param_save(void);          // 保存进存储单元
bool param_reset(void);         // 恢复默认值

#endif // __PARAMETERS_H
