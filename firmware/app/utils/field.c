// ============================================================
// field.c — 统一字段引擎实现
//
// 简化点：
//   - 类型只保留协议实际用到的 4 种（U8 / U32 / F32 / BOOL）
//   - 表项自带字段指针，引擎直接收「表项」而不是「表 + id」
// ============================================================

#include "field.h"

#include <string.h>

uint8_t field_size(uint8_t type)
{
    switch ((eFieldType)type)
    {
    case FLD_U8:
    case FLD_BOOL:
        return 1U;
    case FLD_U32:
    case FLD_F32:
        return 4U;
    default:
        return 0U;
    }
}

float field_read(const tField *f)
{
    if (!f || !f->ptr)
        return 0.0f;

    float v;
    switch ((eFieldType)f->type)
    {
    case FLD_U8:
        v = (float)(*(const uint8_t *)f->ptr);
        break;
    case FLD_U32:
        v = (float)(*(const uint32_t *)f->ptr);
        break;
    case FLD_F32:
        v = *(const float *)f->ptr;
        break;
    case FLD_BOOL:
        v = (*(const bool *)f->ptr) ? 1.0f : 0.0f;
        break;
    default:
        return 0.0f;
    }

    return (f->flags & FF_SCALE) ? (v * f->scale) : v;
}

bool field_write(const tField *f, float v)
{
    if (!f || !f->ptr)
        return false;
    if (!(f->flags & FF_RW))
        return false; // 只读项
    if (v < f->min || v > f->max)
        return false; // 越界

    switch ((eFieldType)f->type)
    {
    case FLD_U8:
        *(uint8_t *)f->ptr = (uint8_t)v;
        break;
    case FLD_U32:
        *(uint32_t *)f->ptr = (uint32_t)v;
        break;
    case FLD_F32:
        *(float *)f->ptr = v;
        break;
    case FLD_BOOL:
        *(bool *)f->ptr = (v != 0.0f);
        break;
    default:
        return false;
    }
    return true;
}

uint8_t field_read_raw(const tField *f, uint8_t *dst)
{
    if (!f || !f->ptr || !dst)
        return 0U;

    uint8_t n = field_size(f->type);
    if (n == 0U)
        return 0U;

    memcpy(dst, f->ptr, n);
    return n;
}

bool field_write_raw(const tField *f, const uint8_t *src, uint8_t len)
{
    if (!f || !f->ptr || !src)
        return false;

    uint8_t n = field_size(f->type);
    if (n == 0U || len != n)
        return false; // 长度不符

    // 原始字节 → float，再统一走 field_write（复用 FF_RW 与范围校验）
    float v;
    switch ((eFieldType)f->type)
    {
    case FLD_U8:
    {
        uint8_t x;
        memcpy(&x, src, 1U);
        v = (float)x;
        break;
    }
    case FLD_U32:
    {
        uint32_t x;
        memcpy(&x, src, 4U);
        v = (float)x;
        break;
    }
    case FLD_F32:
        memcpy(&v, src, 4U);
        break;
    case FLD_BOOL:
        v = (src[0] != 0U) ? 1.0f : 0.0f;
        break;
    default:
        return false;
    }

    return field_write(f, v);
}
