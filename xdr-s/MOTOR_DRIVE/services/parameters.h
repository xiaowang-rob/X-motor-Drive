#ifndef __PARAMETERS_H
#define __PARAMETERS_H

#include "parameter_list.h"
#include "storage.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    void *ptr;    // 字段地址（&结构体.字段）
    uint8_t type; // eFieldType
    float def;    // 默认值
} tField;

typedef struct {
    tParameter value;
    tStorage storage;
    tField fields[PARAM_NUM];
    uint32_t new_firmware;
    uint32_t firmware_size;
} tParamService;

// ---- 读写 / 持久化 ----
// 写入一个参数（value 为该参数的原始字节；id 越界 = "应用并保存" 的协议约定）
void param_set(tParamService *param, eParameter para, uint8_t *value);

// 读取一个参数（value 收原始字节，len 出长度；id 越界时 len=0）
void param_get(const tParamService *param, eParameter para, uint8_t *value, uint8_t *len);

bool param_init(tParamService *param);  // 初始化存储单元、读取 、校验
bool param_save(tParamService *param);  // 保存进存储单元
bool param_reset(tParamService *param); // 恢复默认值

/* 升级元数据保存在参数记录头中；设置/清除只有持久化成功才生效。 */
bool upgrade_save(tParamService *param, uint32_t version, uint32_t size);

#endif // __PARAMETERS_H
