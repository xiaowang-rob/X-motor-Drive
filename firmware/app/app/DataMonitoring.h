#ifndef __DATA_MONITORING_H
#define __DATA_MONITORING_H

#include "bsp_base.h"
#include "protocol.h"
// 读取数据流
void stream_data_get(eData_stream stream, float *data);
void stream_data_prepare(eData_stream stream, u8 index, u8 *data, bool _tx);
#endif