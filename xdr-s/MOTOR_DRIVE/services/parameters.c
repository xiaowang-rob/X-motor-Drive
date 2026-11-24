#include "parameters.h"

#include "bsp_flash.h"
#include <string.h>

#include "crc.h" // crc32（参数持久化校验）

// ============================================================
// ★ 唯一参数定义处
//   X(枚举 id, 结构体字段, 类型, 默认值)
//   - id 直接用 protocol.h 的 eParameter 名 → 与协议强绑定
// ============================================================

// ---- 默认值----
static const tParameter param_default = {
#define P_DEF(id, field, type, def) .field = def,
    PARAM_FIELDS(P_DEF)
#undef P_DEF
};

typedef struct {
    uint32_t new_firmware;
    uint32_t firmware_size;
    uint16_t size;
    uint32_t crc;
} tParamHdr;


bool upgrade_save(tParamService *param, uint32_t version, uint32_t size) {

    param->new_firmware = version;
    param->firmware_size = size;
    if (param_save(param))
        return true;

    return false;
}

#define PARAM_BLOB_SIZE (sizeof(tParamHdr) + sizeof(tParameter))

uint8_t field_read_raw(const tField *f, uint8_t *dst) {
    if (!f || !f->ptr || !dst)
        return 0U;

    uint8_t n = field_value_type_size(f->type);
    memcpy(dst, f->ptr, n);
    return n;
}

bool field_write_raw(const tField *f, const uint8_t *src) {
    if (!f || !f->ptr || !src)
        return false;

    memcpy(f->ptr, src, field_value_type_size(f->type));
    return true;
}

// 参数服务初始化 注册存储单元 读出上次数值
bool param_init(tParamService *param) {
   if (!storage_init(&param->storage, Get_ParamPartHandle()))
       return false;

#define P_DESC(id, field, type, def) \
    param->fields[id] = (tField){&param->value.field, type, (def)};
    PARAM_FIELDS(P_DESC)
#undef P_DESC
    param->value = param_default; // 唯一默认值来源

    uint8_t blob[PARAM_BLOB_SIZE];
    if (storage_read(&param->storage, blob, PARAM_BLOB_SIZE)) {
        tParamHdr hdr;
        memcpy(&hdr, blob, sizeof(hdr));
        if (hdr.size == (uint16_t)sizeof(tParameter)) {
            uint32_t crc = crc32((const uint8_t *)(blob + sizeof(hdr)),
                                 (uint32_t)sizeof(tParameter));
            if (crc == hdr.crc) // CRC 校验（掉电半写可检测）
            {
                memcpy(&param->value, blob + sizeof(hdr), sizeof(tParameter));
            }
        }
    }
    return true;
}
// 保存参数到存储单元
bool param_save(tParamService *param) {
    uint8_t blob[PARAM_BLOB_SIZE];
    tParamHdr hdr = {
        .new_firmware = param->new_firmware,
        .firmware_size = param->firmware_size,
        .size = (uint16_t)sizeof(tParameter),
        .crc = crc32((const uint8_t *)&param->value, (uint32_t)sizeof(tParameter)),
    };
    memcpy(blob, &hdr, sizeof(hdr));
    memcpy(blob + sizeof(hdr), &param->value, sizeof(tParameter));

    return storage_append(&param->storage, blob, PARAM_BLOB_SIZE);
}
// 重置参数到默认值
bool param_reset(tParamService *param) {
    param->value = param_default;
    return param_save(param);
}

// ---- 参数读写（走 field 引擎）----
void param_set(tParamService *param, eParameter para, uint8_t *value) {
    if (para >= PARAM_NUM) {
        // TODO: 协议既有约定：越界 id = "参数应用 "
        return;
    }
    if (!value)
        return;

    field_write_raw(&param->fields[para], value);
}

void param_get(const tParamService *param, eParameter para, uint8_t *value, uint8_t *len) {
    if (para >= PARAM_NUM || !value) {
        if (len)
            *len = 0U;
        return;
    }

    uint8_t n = field_read_raw(&param->fields[para], value);
    if (len)
        *len = n;
}
