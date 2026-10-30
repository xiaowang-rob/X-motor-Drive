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
} eFieldValueType;

#endif
