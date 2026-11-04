/* 两端通用 payload 编解码引擎；唯一输入是 descriptor.h 的描述表。
 * 手写维护：新增字段只需改 protocol 下的 yaml，本文件不需要动。 */
#ifndef PROTO_PAYLOAD_CODEC_H
#define PROTO_PAYLOAD_CODEC_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "descriptor.h"

/* 有符号 4 位半字节；bit3 为符号位。 */
static inline int32_t _proto_nibble(const tProtoField *f, const uint8_t *packet)
{
    uint8_t mask = (uint8_t)((1u << (f->hi - f->lo + 1u)) - 1u);
    int8_t raw = (int8_t)((packet[f->offset] >> f->lo) & mask);
    return (raw & 0x08) ? (int32_t)(raw - 16) : (int32_t)raw;
}

/* 线上原始整数；24 位按需做符号扩展。 */
static inline int32_t _proto_raw(const tProtoField *f, const uint8_t *packet)
{
    if (f->type == FLD_I4)
        return _proto_nibble(f, packet);
    if (f->type == FLD_I24 || f->type == FLD_U24) {
        uint32_t raw = (uint32_t)packet[f->offset]
                       | ((uint32_t)packet[f->offset + 1u] << 8)
                       | ((uint32_t)packet[f->offset + 2u] << 16);
        if (f->type == FLD_I24 && (raw & 0x800000u))
            raw |= 0xFF000000u;
        return (int32_t)raw;
    }
    /* 小端读入；有符号类型必须按线上宽度做符号扩展，否则负值会被当成大正数。 */
    int32_t raw = 0;
    memcpy(&raw, packet + f->offset, f->size);
    switch (f->type) {
    case FLD_U8:
    case FLD_BOOL:
        return (int32_t)(uint8_t)raw;
    case FLD_U16:
        return (int32_t)(uint16_t)raw;
    case FLD_I16:
        return (int32_t)(int16_t)(uint16_t)raw;
    case FLD_U32:
        return (int32_t)(uint32_t)raw;
    default:
        return raw;
    }
}

static inline double _proto_pow2(int exponent)
{
    double factor = 1.0;
    for (int i = 0; i < exponent; ++i)
        factor *= 2.0;
    for (int i = 0; i > exponent; --i)
        factor *= 0.5;
    return factor;
}

/* scale_ref 的线上值就是正指数；scale_exp_ref 的线上值是负指数。 */
static inline double _proto_ref_exponent(const tProtoField *fields, const tProtoField *f,
                                         const uint8_t *packet, bool negate)
{
    int32_t raw = _proto_raw(&fields[f->ref], packet);
    return (double)(negate ? -raw : raw);
}

static inline double proto_field_decode(const tProtoField *fields, const tProtoField *f,
                                        const uint8_t *packet)
{
    if (f->type == FLD_F32) {
        float value;
        memcpy(&value, packet + f->offset, sizeof(value));
        return (double)value;
    }
    int32_t raw = _proto_raw(f, packet);
    switch (f->scale) {
    case PROTO_SCALE_POW2:
        return (double)raw / _proto_pow2(f->exponent);
    case PROTO_SCALE_SIGNED_EXP:
        return (double)(-raw);
    case PROTO_SCALE_REF_U24: {
        /* 前置 U8 存正指数：value = raw / 2^exponent */
        int32_t exponent = _proto_raw(&fields[f->ref], packet);
        return (double)raw / _proto_pow2(exponent);
    }
    case PROTO_SCALE_REF_EXP: {
        /* 前置 FLD_I4 存负指数：value = raw / 2^(-stored) */
        int32_t stored = _proto_raw(&fields[f->ref], packet);
        return (double)raw / _proto_pow2(-stored);
    }
    case PROTO_SCALE_NONE:
    default:
        return (double)raw;
    }
}

static inline void proto_field_encode(const tProtoField *fields, const tProtoField *f,
                                      uint8_t *packet, double value)
{
    if (f->type == FLD_F32) {
        float raw = (float)value;
        memcpy(packet + f->offset, &raw, sizeof(raw));
        return;
    }
    double scale = 1.0;
    if (f->scale == PROTO_SCALE_POW2)
        scale = _proto_pow2(f->exponent);
    else if (f->scale == PROTO_SCALE_REF_U24)
        scale = _proto_pow2((int)(_proto_ref_exponent(fields, f, packet, f->scale == PROTO_SCALE_REF_EXP)));
    else if (f->scale == PROTO_SCALE_REF_EXP)
        scale = _proto_pow2((int)(_proto_ref_exponent(fields, f, packet, f->scale == PROTO_SCALE_REF_EXP)));
    else if (f->scale == PROTO_SCALE_SIGNED_EXP) {
        /* 负指数写在线上：raw = -exponent */
        int32_t raw = (int32_t)(-value);
        packet[f->offset] = (uint8_t)((packet[f->offset]
                                       & (uint8_t)~(((1u << (f->hi - f->lo + 1u)) - 1u) << f->lo))
                                      | (uint8_t)((raw & 0x0F) << f->lo));
        return;
    }
    double scaled = value * scale;
    double rounded = (scaled >= 0.0) ? scaled + 0.5 : scaled - 0.5;
    int32_t raw = (int32_t)rounded;
    if (f->saturate) {
        uint8_t bits = (uint8_t)(f->size * 8u);
        if (f->hi)
            bits = (uint8_t)(f->hi - f->lo + 1u);
        int32_t low = (f->type == FLD_I4 || f->type == FLD_I16 || f->type == FLD_I24)
                      ? -(1 << (bits - 1)) : 0;
        int32_t high = (f->type == FLD_I4 || f->type == FLD_I16 || f->type == FLD_I24)
                       ? (1 << (bits - 1)) - 1 : (int32_t)((1u << bits) - 1u);
        if (raw < low)
            raw = low;
        if (raw > high)
            raw = high;
    }
    if (f->type == FLD_I4) {
        uint8_t shift = f->lo;
        uint8_t mask = (uint8_t)(((1u << (f->hi - f->lo + 1u)) - 1u) << shift);
        packet[f->offset] = (uint8_t)((packet[f->offset] & (uint8_t)~mask)
                                      | (uint8_t)(((uint32_t)raw << shift) & mask));
        return;
    }
    if (f->type == FLD_I24 || f->type == FLD_U24) {
        uint32_t bits = (uint32_t)raw & 0xFFFFFFu;
        packet[f->offset] = (uint8_t)(bits & 0xFFu);
        packet[f->offset + 1u] = (uint8_t)((bits >> 8) & 0xFFu);
        packet[f->offset + 2u] = (uint8_t)((bits >> 16) & 0xFFu);
        return;
    }
    memcpy(packet + f->offset, &raw, f->size);
}

/* 解析实际使用的布局；带变体的命令按首字节 selector 选择。
 * 变体表命中时返回对应分支；标记了变体但未命中时返回 NULL，
 * 由调用方按非法输入处理（与手写宏的长度校验语义一致）。 */
static inline const tProtoLayout *proto_resolve(eProtoLayout layout, const uint8_t *packet)
{
    const tProtoLayout *base = &g_proto_layouts[layout];
    bool has_variant = false;
    for (uint16_t i = 0U; i < PROTO_VARIANT_COUNT; ++i) {
        if (g_proto_variants[i].base != base)
            continue;
        has_variant = true;
        const tProtoField *selector = &g_proto_fields[base->start];
        if (packet[selector->offset] == g_proto_variants[i].selector)
            return g_proto_variants[i].variant;
    }
    return has_variant ? NULL : base;
}

/* 解包到 values[]，返回完整 payload 字节数；变体未命中时返回 0，
 * 但仍按基础布局填充（首字节即 selector，调用方仍可读到模式值）。 */
static inline uint8_t proto_decode(eProtoLayout layout, const uint8_t *packet, double *values)
{
    const tProtoLayout *resolved = proto_resolve(layout, packet);
    bool matched = resolved != NULL;
    if (!resolved)
        resolved = &g_proto_layouts[layout];
    const tProtoField *fields = &g_proto_fields[resolved->start];
    for (uint8_t i = 0U; i < resolved->count; ++i)
        values[i] = proto_field_decode(fields, &fields[i], packet);
    return matched ? resolved->fixed : 0U;
}

/* 从 values[] 装包，返回写入的字节数。 */
static inline uint8_t proto_encode(eProtoLayout layout, uint8_t *packet, const double *values)
{
    const tProtoLayout *target = &g_proto_layouts[layout];
    const tProtoField *fields = &g_proto_fields[target->start];
    memset(packet, 0, target->fixed);
    for (uint8_t i = 0U; i < target->count; ++i)
        proto_field_encode(fields, &fields[i], packet, values[i]);
    return target->fixed;
}

#endif
