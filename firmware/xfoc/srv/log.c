#include "log.h"

#include "bsp_mcu.h"

tFlashUnit log_unit; // 日志存储单元

// TODO:直接通过 存储字符串的方式实现日志记录，再重定向printf

// // 日志服务初始化：注册存储单元，回读最后一条有效日志
// bool log_init(tFlash *flash) {
//     if (!flash_unit_register(flash, &log_unit))
//         return false;

//     // 无文件头：每条就是 sizeof(tLog) 字节，单元为空时读失败即保持默认值
//     flash_unit_read(&log_unit, (uint8_t *)&g_log, (uint32_t)sizeof(g_log));
//     return true;
// }

// // 采集当前运行量填入 g_log
// void log_data_save(tProtectionManager *pro_manager) {
//     if (pro_manager) {
//         g_log.fault = (uint8_t)pro_manager->fault;
//         g_log.warning = (uint8_t)pro_manager->warning;
//     }

//     // 时间戳：由系统运行时间（ms）换算成 hh:mm
//     uint32_t sec = bsp_time_ms() / 1000U;
//     g_log.hours = (uint8_t)((sec / 3600U) % 100U);
//     g_log.minutes = (uint8_t)((sec / 60U) % 60U);

//     // TODO: 电气量（vbus / temp / iu..iw / id / iq / vel / position
//     // 及其目标值）
//     //       由调用方或 foc/core 侧数据源采集后填入 g_log 对应字段
// }

// // 把当前 g_log 作为一条记录追加写入存储单元
// void log_data_write(void) {
//     flash_unit_append(
//         &log_unit, (const uint8_t *)&g_log, (uint32_t)sizeof(g_log));
// }

// // 读指定log
// void log_get(eLogList id, uint8_t *value, uint32_t *len) {
//     if (id >= LOG_NUM) {
//         *len = 0;
//         return;
//     }
//     uint8_t n = field_read_raw(&g_log_fields[id], value);
//     if (len)
//         *len = n;
// }