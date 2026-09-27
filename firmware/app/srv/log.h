#ifndef __LOG_H
#define __LOG_H

#include "protocol.h"
#include "field.h"
#include "protection.h"
#include "flash.h"

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// log.h — 日志服务（srv）
//
// 一条日志记录 = 一个 tLog 结构体，直接整体写入 flash 存储单元，
// 不做 parameters 那种 (size + crc) 文件头；读取时按定长顺序取出。
// 字段 id 与协议枚举 eLogList 一一对应（见 protocol.h）。
// ============================================================

typedef struct
{
    uint8_t hours;
    uint8_t minutes;
    uint8_t fault;
    uint8_t warning;

    float vbus;
    float temp;
    float iu;
    float iv;
    float iw;
    float id;
    float iq;
    float id_tag;
    float iq_tag;
    float vel;
    float vel_tag;
    float position;
    float position_tag;
} tLog;

// 当前待写入 / 已读回的日志记录
extern tLog g_log;

// 日志字段描述符表（[eLogList] 索引；协议按 id 读写单字段用）
extern const tField g_log_fields[LOG_NUM];

bool log_init(tFlash *flash);                        // 注册存储单元并回读最后一条日志
void log_data_save(tProtectionManager *pro_manager); // 采集当前运行量填入 g_log
void log_data_write(void);                           // 把 g_log 作为一条记录追加进 flash

#endif // __LOG_H
