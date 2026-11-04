"""协议 payload 的通用编解码引擎；唯一输入是生成的 descriptor.py。

与固件侧 firmware/protocol/payload_codec.h 一一对应：
缩放、位域、24 位符号扩展、变体分支都只在这里解释一次，
新增字段只改 protocol 下的 yaml，本文件不需要动。
"""
import math
import struct

from protocol import descriptor as spec

__all__ = ["FieldError", "decode", "encode", "layout_of", "resolve"]

# 线上原始值的符号性：U32 超出 int32 的部分按无符号解释。
_SIGNED = {"FLD_I4", "FLD_I16", "FLD_I24"}
_WIDTH = {"FLD_U8": 1, "FLD_BOOL": 1, "FLD_U16": 2, "FLD_U32": 4,
          "FLD_F32": 4, "FLD_I4": 1, "FLD_I16": 2, "FLD_I24": 3, "FLD_U24": 3}
_STATIC = {"FLD_U8", "FLD_BOOL", "FLD_U16", "FLD_U32", "FLD_F32",
           "FLD_I4", "FLD_I16", "FLD_I24", "FLD_U24"}


class FieldError(ValueError):
    """字段越界、类型不匹配或 payload 长度错误。"""


def layout_of(name):
    try:
        return spec.LAYOUTS[name]
    except KeyError:
        raise FieldError(f"未知布局：{name}") from None


def _fields(layout):
    start, count = layout["start"], layout["count"]
    return spec.FIELDS[start:start + count]


def resolve(name, packet):
    """按首字节 selector 选择实际布局；无变体返回原布局，未命中返回 None。"""
    layout = layout_of(name)
    table = spec.VARIANTS.get(name)
    if not table:
        return layout
    selector = _fields(layout)[0]
    if selector["offset"] >= len(packet):
        return None
    target = table.get(packet[selector["offset"]])
    return layout_of(target) if target else None


def _raw(field, packet):
    """线上原始整数（F32 返回 float）。"""
    typ, offset, size = field["type"], field["offset"], field["size"]
    if offset + size > len(packet):
        raise FieldError(f"字段 {field['name']} 数据不足")
    chunk = packet[offset:offset + size]
    if typ == "FLD_F32":
        return struct.unpack("<f", chunk)[0]
    if typ == "FLD_I4":
        shift = field["lo"]
        width = field["hi"] - shift + 1
        value = (chunk[0] >> shift) & ((1 << width) - 1)
        return value - (1 << width) if value & (1 << (width - 1)) else value
    return int.from_bytes(chunk, "little", signed=typ in _SIGNED)


def _ref(field, fields, packet):
    return _raw(fields[field["ref"]], packet)


def field_decode(field, fields, packet):
    """把单个字段解码为工程值（float）。"""
    kind = field["scale"]
    if field["type"] == "FLD_F32":
        value = _raw(field, packet)
        if not math.isfinite(value):
            raise FieldError(f"字段 {field['name']} 不是有限数")
        return value
    raw = _raw(field, packet)
    if kind == spec.SCALE_POW2:
        return raw / float(1 << field["exponent"])
    if kind == spec.SCALE_SIGNED_EXP:
        return float(-raw)
    if kind == spec.SCALE_REF_U24:
        return raw / float(1 << _ref(field, fields, packet))
    if kind == spec.SCALE_REF_EXP:
        # 线上存负指数：value = raw / 2^(-stored)
        return raw / float(1 << -_ref(field, fields, packet))
    return float(raw)


def _integer(value, field):
    if isinstance(value, bool):
        raise FieldError(f"字段 {field['name']} 不接受布尔值")
    value = float(value)
    if not math.isfinite(value):
        raise FieldError(f"字段 {field['name']} 必须有限")
    raw = math.floor(value + 0.5) if value >= 0 else math.ceil(value - 0.5)
    if field["saturate"]:
        bits = _WIDTH[field["type"]] * 8
        if field["type"] == "FLD_I4":
            bits = field["hi"] - field["lo"] + 1
        low = -(1 << (bits - 1)) if field["type"] in _SIGNED else 0
        high = (1 << (bits - 1)) - 1 if field["type"] in _SIGNED else (1 << bits) - 1
        return min(high, max(low, raw))
    return raw


def field_encode(field, fields, packet, value):
    """把工程值写入 packet；越界抛 FieldError。"""
    typ, offset, size = field["type"], field["offset"], field["size"]
    if typ not in _STATIC:
        raise FieldError(f"字段 {field['name']} 类型 {typ} 不支持静态写入")
    if offset + size > len(packet):
        raise FieldError(f"字段 {field['name']} 缓冲区不足")
    if typ == "FLD_F32":
        value = float(value)
        if not math.isfinite(value):
            raise FieldError(f"字段 {field['name']} 必须有限")
        packet[offset:offset + size] = struct.pack("<f", value)
        return
    kind = field["scale"]
    if kind == spec.SCALE_SIGNED_EXP:
        # 线上存负指数；直接写原始值 -value。
        raw = -int(round(float(value)))
    else:
        scale = 1.0
        if kind == spec.SCALE_POW2:
            scale = float(1 << field["exponent"])
        elif kind == spec.SCALE_REF_U24:
            # 前置 U8 存正指数：raw = value * 2^stored
            scale = float(1 << _ref(field, fields, packet))
        elif kind == spec.SCALE_REF_EXP:
            # 前置 I4 存负指数：raw = value * 2^(-stored)
            scale = float(1 << -_ref(field, fields, packet))
        raw = _integer(float(value) * scale, field)
    if typ == "FLD_I4":
        shift = field["lo"]
        width = field["hi"] - shift + 1
        mask = ((1 << width) - 1) << shift
        packet[offset] = (packet[offset] & ~mask) | ((raw << shift) & mask)
        return
    if typ in _SIGNED:
        low, high = -(1 << (size * 8 - 1)), (1 << (size * 8 - 1)) - 1
        if not low <= raw <= high:
            raise FieldError(f"字段 {field['name']} 整数越界")
    else:
        if not 0 <= raw < (1 << (size * 8)):
            raise FieldError(f"字段 {field['name']} 整数越界")
    packet[offset:offset + size] = raw.to_bytes(size, "little", signed=typ in _SIGNED)


def decode(name, packet):
    """解出 {字段名: 工程值}；变体未命中抛 FieldError。"""
    layout = resolve(name, packet)
    if layout is None:
        raise FieldError(f"{name} 的模式值不在协议表内")
    fields = _fields(layout)
    if len(packet) != layout["fixed"]:
        raise FieldError(f"payload 长度错误：需要 {layout['fixed']}，实际 {len(packet)}")
    return {field["name"]: field_decode(field, fields, packet) for field in fields}


def encode(name, values):
    """按字段名装包；缺省字段写 0。"""
    layout = layout_of(name)
    fields = _fields(layout)
    packet = bytearray(layout["fixed"])
    for field in fields:
        if field["type"] not in _STATIC:
            continue
        field_encode(field, fields, packet, values.get(field["name"], 0))
    return bytes(packet)
