#ifndef __DATA_H
#define __DATA_H

#include "protocol.h"
#include <stdint.h>
#include <stdbool.h>

// 读通道（已换算到工程单位）
float dm_data_get(eDataList stream);

// 写通道；返回 false 表示该项只读 / id 越界 / 数值超范围
bool dm_data_set(eDataList stream, float value);

// 打包：把通道放进缓冲第 index 位；_tx=true 时整包拷出
void dm_data_prepare(eDataList stream, uint8_t index, uint8_t *data, bool _tx);

#endif