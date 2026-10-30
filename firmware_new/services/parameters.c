#include "parameters.h"

#include "crc.h" // crc32（参数持久化校验）

tParameter g_param;
tFlashUnit param_unit;

// ============================================================
// ★ 唯一参数定义处
//   X(枚举 id, 结构体字段, 类型, 默认值)
//   - id 直接用 protocol.h 的 eParameter 名 → 与协议强绑定
// ============================================================

// ---- 描述符表（协议读写用；字段名错 → 编译期报错）----
const tField g_param_fields[PARAM_NUM] = {
#define P_DESC(id, field, type, def) \
    [id] = {&g_param.field, type, (def)},
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

static const uint8_t TYPE_SIZE[] = {
    [FLD_U8] = sizeof(uint8_t),
    [FLD_U32] = sizeof(uint32_t),
    [FLD_F32] = sizeof(float),
    [FLD_BOOL] = sizeof(bool),
};

uint8_t field_read_raw(const tField *f, uint8_t *dst)
{
    if (!f || !f->ptr || !dst)
        return 0U;

    uint8_t n = TYPE_SIZE[f->type];
    memcpy(dst, f->ptr, n);
    return n;
}

bool field_write_raw(const tField *f, const uint8_t *src)
{
    if (!f || !f->ptr || !src)
        return false;

    memcpy(f->ptr, src, TYPE_SIZE[f->type]);
    return true;
}

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
