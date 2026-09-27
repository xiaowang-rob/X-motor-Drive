// ============================================================
// field.c — 统一字段引擎实现
// ============================================================

#include "field.h"

#include <string.h>

const uint8_t TYPE_SIZE[] = {
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
