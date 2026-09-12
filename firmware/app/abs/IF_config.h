#ifndef __IF_CONFIG_H
#define __IF_CONFIG_H

// 配置参数接口 需要在驱动层定义对应的驱动的具体配置

// ---------- 固件版本与作者 ----------
#define AUTHOR "wxd"
extern const char XDR_VERSION[];

// ---------- 保护阈值 ----------
extern const float MAX_CURRENT;
extern const float MAX_VOLTAGE;
extern const float MIN_VOLTAGE;
extern const float MAX_TEMPERATURE;

// ---------- 数据流 ----------
#define T_DATA_STREAM 1     // 数据流发送间隔 (ms)
#define T_STATE_STREAM 500  // 状态包发送间隔 (ms)
#define TEMP_VBUS_TS_MS 300 // 温度/电压采样间隔 (ms)

#endif