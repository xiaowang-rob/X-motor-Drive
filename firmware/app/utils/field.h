#ifndef __FIELD_H
#define __FIELD_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// field.h — 统一字段描述符（参数与数据共用一套引擎）
//
// 概念：
//   "参数" 与 "数据" 是**用途**分类（参数＝配置量、数据＝运行量）；
//   "可读 / 可写" 是**权限**属性 —— 两者正交：
//     数据里既有只读反馈量（iu/iq_fb/theta…），也有可写参考量（iq_ref/pos_ref…）。
//
// 所以两者共用同一个 tField 与同一个读写引擎，差异只在每项的 flags：
//     FF_RW     该项可被写入（缺省＝只读）
//     FF_SCALE  读出时 ×scale 换算为工程单位
//
// 要点：
//   - 表项直接存**字段指针**（&g_Param.xxx），字段名写错编译期即报错；
//   - 表是 const，运行期只读遍历：零注册、零查找、零分配。
//
// 用法：
//     static const tField s_param_fields[PARAM_NUM] = {
//     #define X(id, field, type, min, max, flags) \
//         [id] = { &g_Param.field, FLD_##type, FF_##flags, (min), (max), 1.0f },
//         PARAM_FIELDS(X)
//     #undef X
//     };
// ============================================================

// 只保留协议实际用到的 4 种（需要时再扩）
typedef enum
{
    FLD_U8 = 0,
    FLD_U32,
    FLD_F32,
    FLD_BOOL,
} eFieldType;

#define FF_RW (1u << 0)    // 该项可被写入（缺省＝只读）
#define FF_SCALE (1u << 1) // 读出按 scale 换算为工程单位
#define FF_RO 0u           // 只读（无位），用于表里显式标注

typedef struct
{
    void *ptr;       // 字段地址（&结构体.字段）
    uint8_t type;    // eFieldType
    uint8_t flags;   // FF_*
    float min;       // 写入下限（FF_RW 有效）
    float max;       // 写入上限（FF_RW 有效）
    float scale;     // 工程值 = 原始 × scale（FF_SCALE 有效）
} tField;

// 类型名映射：供 X-Macro 展开结构体 / 默认值时把类型标签转成 C 类型
#define FIELD_CTYPE_U8 uint8_t
#define FIELD_CTYPE_U32 uint32_t
#define FIELD_CTYPE_F32 float
#define FIELD_CTYPE_BOOL bool

// ---- 引擎 ----

uint8_t field_size(uint8_t type);                       // 类型字节数（协议打包/校验）
float field_read(const tField *f);                      // 读（含 scale）
bool field_write(const tField *f, float v);             // 写（含 FF_RW 与 min/max 校验）
uint8_t field_read_raw(const tField *f, uint8_t *dst);  // 读原始字节，返回长度
bool field_write_raw(const tField *f, const uint8_t *src, uint8_t len); // 写原始字节

#endif // __FIELD_H
