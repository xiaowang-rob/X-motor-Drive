#include "data.h"

// ============================================================
// ★ 唯一通道定义处
//   X(枚举 id, tFOC_val 字段, 类型, 换算系数, 最小, 最大, 权限)
//   scale：读出值 = 原始值 × scale（不需要换算填 1.0f）
//   权限：RW 可写（参考量） / RO 只读（反馈量）
//   范围：仅对 RW 项有意义，是"粗限幅"（挡住明显越界值）；
//         精确限幅仍由控制层按 limit_current / limit_vel / limit_position_* 执行。
// ============================================================
#define STREAM_FIELDS(X)                                         \
    X(CURRENT_U, iu, F32, 1.0f, 0.0f, 0.0f, RO)                  \
    X(CURRENT_V, iv, F32, 1.0f, 0.0f, 0.0f, RO)                  \
    X(CURRENT_W, iw, F32, 1.0f, 0.0f, 0.0f, RO)                  \
    X(VOLTAGE_Q, uq, F32, 1.0f, 0.0f, 0.0f, RO)                  \
    X(VOLTAGE_D, ud, F32, 1.0f, 0.0f, 0.0f, RO)                  \
    X(CURRENT_ALPHA, ialpha, F32, 1.0f, 0.0f, 0.0f, RO)          \
    X(CURRENT_BETA, ibeta, F32, 1.0f, 0.0f, 0.0f, RO)            \
    X(CURRENT_Q, iq_fb, F32, 1.0f, 0.0f, 0.0f, RO)               \
    X(CURRENT_D, id_fb, F32, 1.0f, 0.0f, 0.0f, RO)               \
    X(CURRENT_Q_REF, iq_ref, F32, 1.0f, -200.0f, 200.0f, RW)     \
    X(CURRENT_D_REF, id_ref, F32, 1.0f, -200.0f, 200.0f, RW)     \
    X(VELOCITY, vel_fb, F32, 1.0f, 0.0f, 0.0f, RO)               \
    X(VELOCITY_REF, vel_ref, F32, 1.0f, -10000.0f, 10000.0f, RW) \
    X(THETA_ELEC, theta_elec, F32, 1.0f, 0.0f, 0.0f, RO)         \
    X(THETA_MECH, theta_mech, F32, 1.0f, 0.0f, 0.0f, RO)         \
    X(POSITION, pos_fb, F32, 1.0f, 0.0f, 0.0f, RO)               \
    X(POSITION_REF, pos_ref, F32, 1.0f, -1.0e9f, 1.0e9f, RW)

// 通道描述符表（FF_SCALE 统一附加；范围/换算来自 LIST）
static const tField s_stream_fields[DATA_NUM] = {
#define S_DESC(id, field, type, sc, lo, hi, fl) \
    [id] = {&foc_val.field, FLD_##type, FF_##fl | FF_SCALE, (lo), (hi), (sc)},
    STREAM_FIELDS(S_DESC)
#undef S_DESC
};

// 打包缓冲：把连续通道拼成待发字节流（须持久，DMA 发送期间不能被覆盖）
static float s_pack_buf[DATA_NUM];

float dm_data_get(eDataList stream)
{
    if ((uint16_t)stream >= DATA_NUM)
        return 0.0f;
    return field_read(&s_stream_fields[stream]);
}

bool dm_data_set(eDataList stream, float value)
{
    if ((uint16_t)stream >= DATA_NUM)
        return false;
    return field_write(&s_stream_fields[stream], value); // 引擎内含 FF_RW 与范围校验
}

void dm_data_prepare(eDataList stream, uint8_t index, uint8_t *data, bool _tx)
{
    if (!data || index >= DATA_NUM)
        return;

    s_pack_buf[index] = field_read(&s_stream_fields[stream]);
    if (_tx)
        memcpy(data, s_pack_buf, (uint32_t)(index + 1U) * sizeof(float));
}
