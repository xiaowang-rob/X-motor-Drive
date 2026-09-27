#include "data.h"

#include <string.h>

// TODO: 数据源绑定表：各成员指向实时数据地址，由组装层绑定（只读）
tData g_data;

// ============================================================
// ★ 唯一通道定义处
//   X(枚举 id, tData 字段)
//   - id 直接用 protocol.h 的 eDataList 名 → 与协议强绑定
// ============================================================
#define DATA_FIELDS(X)        \
    X(CURRENT_U, iu)          \
    X(CURRENT_V, iv)          \
    X(CURRENT_W, iw)          \
    X(VOLTAGE_Q, uq)          \
    X(VOLTAGE_D, ud)          \
    X(CURRENT_ALPHA, ialpha)  \
    X(CURRENT_BETA, ibeta)    \
    X(CURRENT_Q, iq)          \
    X(CURRENT_D, id)          \
    X(CURRENT_Q_REF, iq_ref)  \
    X(CURRENT_D_REF, id_ref)  \
    X(VELOCITY, vel)          \
    X(VELOCITY_REF, vel_ref)  \
    X(THETA_ELEC, theta_elec) \
    X(THETA_MECH, theta_mech) \
    X(POSITION, pos)          \
    X(POSITION_REF, pos_ref)

// 槽位表：[eDataList] → &g_data.xxx（槽里存的才是绑定的实时数据地址）
// 只读：本模块不（也无需）改写 g_data.xxx 指向的内容
static const float **const s_data_slots[DATA_NUM] = {
#define D_SLOT(id, field) [id] = &g_data.field,
    DATA_FIELDS(D_SLOT)
#undef D_SLOT
};

// 读一个通道：解引用绑定地址；未绑定返回 0.0f
float dm_data_get(eDataList stream)
{
    if ((uint16_t)stream >= DATA_NUM)
        return 0.0f;

    const float *p = *s_data_slots[stream];
    return p ? *p : 0.0f;
}
