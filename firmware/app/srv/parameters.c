#include "parameters.h"
#include "flash.h"

#include "crc.h" // crc32（参数持久化校验）

#include <string.h>

tParameter g_param;
tFlashUnit param_unit;

// ============================================================
// ★ 唯一参数定义处
//   X(枚举 id, 结构体字段, 类型, 默认值, 最小, 最大, 标志)
//   - id 直接用 protocol.h 的 eParameter 名 → 与协议强绑定，书写顺序无关
//   - 标志：RW 可写（本工程参数均可写）
// ============================================================
#define PARAM_FIELDS(X)                                                        \
    X(INC_ENC_MODE, ienc_mode, U8, 0u, 0.0f, 2.0f, RW)                         \
    X(EXT_ENC_MODE, eenc_mode, U8, 0u, 0.0f, 2.0f, RW)                         \
    X(EXT_ENC_CHIP, eenc_chip, U8, 0u, 0.0f, 2.0f, RW)                         \
    X(OBS_MODE, obs_mode, U8, 0u, 0.0f, 3.0f, RW)                              \
    X(RUN_MODE, run_mode, U8, 0u, 0.0f, 5.0f, RW)                              \
    X(CAN_MODE, can_mode, U8, 0u, 0.0f, 1.0f, RW)                              \
    X(TRAJ_TYPE, traj_type, U8, 0u, 0.0f, 3.0f, RW)                            \
    X(MOTOR_POLEPAIRS, motor_polepairs, U8, 7u, 1.0f, 64.0f, RW)               \
    X(THETA_OFFSET, theta_offset, F32, 0.0f, -3.15f, 3.15f, RW)                \
    X(POSITIVE_DIR, positive_dir, BOOL, true, 0.0f, 1.0f, RW)                  \
    X(MOTOR_KV, motor_kv, F32, 0.0f, 0.0f, 10000.0f, RW)                       \
    X(MOTOR_RS, motor_rs, F32, 0.07f, 0.0f, 100.0f, RW)                        \
    X(MOTOR_Ld, motor_ld, F32, 45e-6f, 0.0f, 1.0f, RW)                         \
    X(MOTOR_Lq, motor_lq, F32, 49e-6f, 0.0f, 1.0f, RW)                         \
    X(MOTOR_PSIF, motor_psif, F32, 0.00035f, 0.0f, 10.0f, RW)                  \
    X(MOTOR_KE, motor_ke, F32, 0.0025f, 0.0f, 10.0f, RW)                       \
    X(MOTOR_J, motor_j, F32, 1e-4f, 0.0f, 1.0f, RW)                            \
    X(MOTOR_B, motor_b, F32, 1e-3f, 0.0f, 1.0f, RW)                            \
    X(CAN_ID, can_id, U32, 1u, 0.0f, 2047.0f, RW)                              \
    X(CLBW, clbw, F32, 0.0f, 0.0f, 10000.0f, RW)                               \
    X(VLKP, vlkp, F32, 3.0f, 0.0f, 10000.0f, RW)                               \
    X(VLKI, vlki, F32, 20.0f, 0.0f, 10000.0f, RW)                              \
    X(PLKP, plkp, F32, 0.5f, 0.0f, 10000.0f, RW)                               \
    X(PLKI, plki, F32, 0.05f, 0.0f, 10000.0f, RW)                              \
    X(PLKD, plkd, F32, 0.005f, 0.0f, 10000.0f, RW)                             \
    X(PLALPHA, plalpha, F32, 0.15f, 0.0f, 1.0f, RW)                            \
    X(MIT_KP, mit_kp, F32, 0.0f, 0.0f, 10000.0f, RW)                           \
    X(MIT_KD, mit_kd, F32, 0.0f, 0.0f, 10000.0f, RW)                           \
    X(MIT_TMAX, mit_tmax, F32, 0.0f, 0.0f, 1000.0f, RW)                        \
    X(TUNE_CURRENT, tune_current, F32, 1.5f, 0.0f, 100.0f, RW)                 \
    X(LIMIT_CURRENT, limit_current, F32, 30.0f, 0.0f, 200.0f, RW)              \
    X(LIMIT_VELOCITY, limit_vel, F32, 209.44f, 0.0f, 10000.0f, RW)             \
    X(LIMIT_POSITION_MIN, limit_position_min, F32, -15000.0f, -1e9f, 1e9f, RW) \
    X(LIMIT_POSITION_MAX, limit_position_max, F32, 15000.0f, -1e9f, 1e9f, RW)  \
    X(TOLERANCE_TIME, tolerance_time, F32, 1.0f, 0.0f, 1000.0f, RW)            \
    X(TOLERANCE_LIMIT, tolerance_limit, F32, 1.1f, 0.0f, 1000.0f, RW)          \
    X(TRAJ_LIMIT_D1, traj_limit_d1, F32, 1000.0f, 0.0f, 1e9f, RW)              \
    X(TRAJ_LIMIT_D2, traj_limit_d2, F32, 1000.0f, 0.0f, 1e9f, RW)              \
    X(TRAJ_LIMIT_D3, traj_limit_d3, F32, 1000.0f, 0.0f, 1e9f, RW)              \
    X(TRAJ_TOLERANCE, tolerance, F32, 0.1f, 0.0f, 1e9f, RW)

// 内部参数：不进协议（不在 eParameter 里），但需要默认值
#define INTERNAL_FIELDS(X) \
    X(qclkp, F32, 1.1f)    \
    X(qclki, F32, 1.1f)    \
    X(dclkp, F32, 1.1f)    \
    X(dclki, F32, 600.0f)  \
    X(wlkp, F32, 1.1f)     \
    X(wlki, F32, 1.1f)     \
    X(cfalpha, F32, 0.4f)  \
    X(vfalpha, F32, 0.08f)

// ---- 描述符表（协议读写用；字段名错 → 编译期报错）----
const tField g_param_fields[PARAM_NUM] = {
#define P_DESC(id, field, type, def, lo, hi, fl) \
    [id] = {&g_param.field, FLD_##type, FF_##fl, (lo), (hi), 1.0f},
    PARAM_FIELDS(P_DESC)
#undef P_DESC
};

// ---- 默认值（同一份 LIST，不再手写第二遍）----
static const tParameter s_param_default = {
#define P_DEF(id, field, type, def, lo, hi, fl) .field = (FIELD_CTYPE_##type)(def),
    PARAM_FIELDS(P_DEF)
#undef P_DEF
#define I_DEF(field, type, def) .field = (FIELD_CTYPE_##type)(def),
        INTERNAL_FIELDS(I_DEF)
#undef I_DEF
};

// ---- 版本化存储：version + size + crc32 + payload ----
//   version 变更 → 回默认值（避免旧布局错位读）；
//   crc32       → 掉电/半写可检测（未写过时为全 FF，CRC 必然不符）
#define PARAM_VERSION 1u /* 结构体布局版本：改字段就 +1 */

typedef struct
{
    uint16_t version;
    uint16_t size;
    uint32_t crc;
} tParamHdr;

#define PARAM_BLOB_SIZE (sizeof(tParamHdr) + sizeof(tParameter))

static tParamStore s_store;

void dm_param_bind_store(const tParamStore *store)
{
    if (store)
        s_store = *store;
}

static uint32_t param_crc(void)
{
    return crc32((const uint8_t *)&g_param, (uint32_t)sizeof(tParameter));
}

bool dm_param_save(void)
{
    if (!s_store.write)
        return false;

    uint8_t blob[PARAM_BLOB_SIZE];
    tParamHdr hdr = {
        .version = PARAM_VERSION,
        .size = (uint16_t)sizeof(tParameter),
        .crc = param_crc(),
    };
    memcpy(blob, &hdr, sizeof(hdr));
    memcpy(blob + sizeof(hdr), &g_param, sizeof(tParameter));
    return s_store.write(blob, PARAM_BLOB_SIZE);
}

bool dm_param_erase(void)
{
    memset(&g_param, 0, sizeof(g_param));
    return s_store.erase ? s_store.erase() : false;
}

bool dm_param_init(void)
{
    memset(&g_param, 0, sizeof(g_param));

    bool ok = false;
    if (s_store.read)
    {
        uint8_t blob[PARAM_BLOB_SIZE];
        if (s_store.read(blob, PARAM_BLOB_SIZE))
        {
            tParamHdr hdr;
            memcpy(&hdr, blob, sizeof(hdr));
            if (hdr.version == PARAM_VERSION &&
                hdr.size == (uint16_t)sizeof(tParameter))
            {
                memcpy(&g_param, blob + sizeof(hdr), sizeof(tParameter));
                ok = (param_crc() == hdr.crc); // CRC 校验（掉电半写可检测）
            }
        }
    }

    if (!ok)
    {
        g_param = s_param_default; // 唯一默认值来源
        dm_param_save();
    }
    return true;
}

// ---- 参数生效钩子：各子系统注册自己的"重配置"动作 ----
#define PARAM_APPLY_MAX 4U
static void (*s_apply[PARAM_APPLY_MAX])(void);
static uint8_t s_apply_cnt;

void dm_param_register_apply(void (*hook)(void))
{
    if (hook && s_apply_cnt < PARAM_APPLY_MAX)
        s_apply[s_apply_cnt++] = hook;
}

void dm_param_apply(void)
{
    for (uint8_t i = 0U; i < s_apply_cnt; i++)
        s_apply[i]();
}

// ---- 参数读写（走 field 引擎）----

void dm_param_set(eParameter para, uint8_t *value)
{
    if (para >= PARAM_NUM)
    {
        // 协议既有约定：越界 id = "参数应用 + 保存"
        dm_param_apply();
        dm_param_save();
        return;
    }
    if (!value)
        return;

    field_write_raw(&g_param_fields[para], value, field_size(g_param_fields[para].type));
}

void dm_param_get(eParameter para, uint8_t *value, uint8_t *len)
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
