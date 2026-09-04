#ifndef __ABS_TIME_H
#define __ABS_TIME_H

#include <stdint.h>

// ============================================================
// time.h — 时间接口类型（usr/abs ↔ 板服务）
//
// v2：abs 业务对象（闪烁/呼吸/采样节流）需要时间基准，经本类型注入。
// 实现由各板提供：可直接用裸函数包装（见 dev_board），
// 或平台提供适配实例。类型归 abs，实现不进 abs。
//
// 规则：返回可回绕，调用方一律用差值比较。
// ============================================================

typedef struct
{
    void *ctx; // 实现侧上下文（对调用方不透明）

    uint32_t (*get_ms)(void *ctx);
    uint32_t (*get_us)(void *ctx);
} tTimeIf;

#endif // __ABS_TIME_H
