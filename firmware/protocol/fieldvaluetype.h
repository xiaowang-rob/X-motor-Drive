/* 此文件由 protocol/gen_protocol.py 自动生成，请勿手动修改 */
#ifndef PROTO_FIELDVALUETYPE_H
#define PROTO_FIELDVALUETYPE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    FLD_U8 = 0, /* uint8_t */
    FLD_U16 = 1, /* uint16_t */
    FLD_U32 = 2, /* uint32_t */
    FLD_F32 = 3, /* float */
    FLD_BOOL = 4, /* bool */
    FLD_I4 = 5, /* int8_t */
    FLD_I16 = 6, /* int16_t */
    FLD_I24 = 7, /* int32_t */
    FLD_U24 = 8, /* uint32_t */
} eFieldValueType;

/* 返回协议中的字节数，不是 C 存储类型的 sizeof；未知类型返回 0。 */
static inline uint8_t field_value_type_size(eFieldValueType type)
{
    switch (type) {
    case FLD_U8: return 1U;
    case FLD_U16: return 2U;
    case FLD_U32: return 4U;
    case FLD_F32: return 4U;
    case FLD_BOOL: return 1U;
    case FLD_I4: return 1U;
    case FLD_I16: return 2U;
    case FLD_I24: return 3U;
    case FLD_U24: return 3U;
    default: return 0U;
    }
}

#endif
