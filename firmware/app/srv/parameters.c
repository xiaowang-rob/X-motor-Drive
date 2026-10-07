#include "parameters.h"

#include "crc.h" // crc32（参数持久化校验）

tParameter g_param;
tFlashUnit param_unit;

// ============================================================
// ★ 唯一参数定义处
//   X(枚举 id, 结构体字段, 类型, 默认值)
//   - id 直接用 protocol.h 的 eParameter 名 → 与协议强绑定
// ============================================================
#define PARAM_FIELDS(X)                                       \
    X(INC_ENC_MODE, ienc_mode, U8, 0u)                        \
    X(INC_ENC_CHIP, ienc_chip, U8, 0u)                        \
    X(EXT_ENC_MODE, eenc_mode, U8, 0u)                        \
    X(EXT_ENC_CHIP, eenc_chip, U8, 0u)                        \
    X(ENC_ACTIVE, enc_active, BOOL, true)                     \
    X(OBS_ACTIVE, obs_active, BOOL, false)                    \
    X(CTRL_MODE, ctrl_mode, U8, 0u)                           \
    X(TRAJ_MODE, traj_type, U8, 0u)                           \
    X(MOTOR_POLEPAIRS, motor_polepairs, U8, 7u)               \
    X(POSITIVE_DIR, positive_dir, BOOL, true)                 \
    X(THETA_OFFSET, theta_offset, F32, 0.0f)                  \
    X(MOTOR_KV, motor_kv, F32, 0.0f)                          \
    X(MOTOR_RS, motor_rs, F32, 0.07f)                         \
    X(MOTOR_Ld, motor_ld, F32, 45e-6f)                        \
    X(MOTOR_Lq, motor_lq, F32, 49e-6f)                        \
    X(MOTOR_PSIF, motor_psif, F32, 0.00035f)                  \
    X(MOTOR_KE, motor_ke, F32, 0.0025f)                       \
    X(MOTOR_J, motor_j, F32, 1e-4f)                           \
    X(MOTOR_B, motor_b, F32, 1e-3f)                           \
    X(CAN_ID, can_id, U32, 1u)                                \
    X(CAN_MODE, can_mode, U8, 0u)                             \
    X(CLBW_COEF, clbw_coef, F32, 0.0f)                        \
    X(QCLKP, qclkp, F32, 0.0f)                                \
    X(QCLKI, qclki, F32, 0.0f)                                \
    X(DCLKP, dclkp, F32, 0.0f)                                \
    X(DCLKI, dclki, F32, 0.0f)                                \
    X(CFALPHA, cfalpha, F32, 0.0f)                            \
    X(VLKP, vlkp, F32, 3.0f)                                  \
    X(VLKI, vlki, F32, 20.0f)                                 \
    X(WLKP, wlkp, F32, 0.0f)                                  \
    X(WLKI, wlki, F32, 0.0f)                                  \
    X(PLKP, plkp, F32, 0.5f)                                  \
    X(PLKI, plki, F32, 0.05f)                                 \
    X(PLKD, plkd, F32, 0.005f)                                \
    X(PLALPHA, plalpha, F32, 0.15f)                           \
    X(MIT_KP, mit_kp, F32, 0.0f)                              \
    X(MIT_KD, mit_kd, F32, 0.0f)                              \
    X(MIT_TSTA, mit_tsta, F32, 0.0f)                          \
    X(MIT_TMAX, mit_tmax, F32, 0.0f)                          \
    X(TUNE_CURRENT, tune_current, F32, 1.5f)                  \
    X(LIMIT_CURRENT, limit_current, F32, 30.0f)               \
    X(LIMIT_VELOCITY, limit_vel, F32, 209.44f)                \
    X(LIMIT_POSITION_MIN, limit_position_min, F32, -15000.0f) \
    X(LIMIT_POSITION_MAX, limit_position_max, F32, 15000.0f)  \
    X(TOLERANCE_TIME, tolerance_time, F32, 1.0f)              \
    X(TOLERANCE_LIMIT, tolerance_limit, F32, 1.1f)            \
    X(TRAJ_LIMIT_D1, traj_limit_d1, F32, 1000.0f)             \
    X(TRAJ_LIMIT_D2, traj_limit_d2, F32, 1000.0f)             \
    X(TRAJ_LIMIT_D3, traj_limit_d3, F32, 1000.0f)             \
    X(TRAJ_TOLERANCE, traj_tolerance, F32, 0.1f)
// ---- 描述符表（协议读写用；字段名错 → 编译期报错）----
const tField g_param_fields[PARAM_NUM] = {
#define P_DESC(id, field, type, def) \
    [id] = {&g_param.field, FLD_##type, (def)},
    PARAM_FIELDS(P_DESC)
#undef P_DESC
};

// ---- 默认值----
static const tParameter param_default = {
#define P_DEF(id, field, type, def) \
    .field = def,
    PARAM_FIELDS(P_DEF)
#undef P_DEF
};

typedef struct
{
    uint16_t size;
    uint32_t crc;
} tParamHdr;

#define PARAM_BLOB_SIZE (sizeof(tParamHdr) + sizeof(tParameter))

// 参数服务初始化 注册存储单元 读出上次数值
bool param_init(tFlash *flash)
{
    if (!flash_unit_register(flash, &param_unit))
        return false;
    g_param = param_default; // 唯一默认值来源

    uint8_t blob[PARAM_BLOB_SIZE];
    if (flash_unit_read(&param_unit, blob, PARAM_BLOB_SIZE))
    {
        tParamHdr hdr;
        memcpy(&hdr, blob, sizeof(hdr));
        if (hdr.size == (uint16_t)sizeof(tParameter))
        {
            uint32_t crc = crc32((const uint8_t *)(blob + sizeof(hdr)), (uint32_t)sizeof(tParameter));
            if (crc == hdr.crc) // CRC 校验（掉电半写可检测）
            {
                memcpy(&g_param, blob + sizeof(hdr), sizeof(tParameter));
            }
            else // 参数有问题 把默认值写入
            {
                param_save();
            }
        }
    }
    return true;
}
// 保存参数到存储单元
bool param_save(void)
{
    uint8_t blob[PARAM_BLOB_SIZE];
    tParamHdr hdr = {
        .size = (uint16_t)sizeof(tParameter),
        .crc = crc32((const uint8_t *)&g_param, (uint32_t)sizeof(tParameter)),
    };
    memcpy(blob, &hdr, sizeof(hdr));
    memcpy(blob + sizeof(hdr), &g_param, sizeof(tParameter));

    return flash_unit_append(&param_unit, blob, PARAM_BLOB_SIZE);
}
// 重置参数到默认值
bool param_reset(void)
{
    g_param = param_default;
    return param_save();
}

// ---- 参数读写（走 field 引擎）----
void param_set(eParameter para, uint8_t *value)
{
    if (para >= PARAM_NUM)
    {
        // TODO: 协议既有约定：越界 id = "参数应用 "
        return;
    }
    if (!value)
        return;

    field_write_raw(&g_param_fields[para], value);
}

void param_get(eParameter para, uint8_t *value, uint8_t *len)
{
    if (para >= PARAM_NUM || !value)
    {
        if (len)
            *len = 0U;
        return;
    }

    uint8_t n = field_read_raw(&g_param_fields[para], value);
    if (len)
        *len = n;
}
