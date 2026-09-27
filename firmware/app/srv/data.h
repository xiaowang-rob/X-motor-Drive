#ifndef __DATA_H
#define __DATA_H

#include "protocol.h"

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// data.h — 运行量数据服务（srv，只读）
//
// tData 不存数据，只保存"实时数据地址的绑定"：每个成员是一个
// const float*，由组装层把它绑到对应的实时变量地址，例如
//     g_data.iu = &foc.val.iu;
// 本模块只解引用读取，不提供改写入口，也不落 flash。
// 通道 id 与协议枚举 eDataList 一一对应（见 protocol.h）。
// ============================================================

typedef struct
{
    const float *iu, *iv, *iw;   // 三相电流 A
    const float *uq, *ud;        // qd 电压 V
    const float *ialpha, *ibeta; // αβ 电流 A
    const float *iq, *id;        // dq 电流 A
    const float *iq_ref, *id_ref;
    const float *vel, *vel_ref;           // 速度 / 速度目标
    const float *theta_elec, *theta_mech; // 电角度 / 机械角度
    const float *pos, *pos_ref;           // 位置 / 位置目标
} tData;

// 数据源绑定表：各成员指向实时数据地址，由组装层绑定（只读）
extern tData g_data;

// 读一个通道（未绑定时返回 0.0f）
float dm_data_get(eDataList stream);

#endif
