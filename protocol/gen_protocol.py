#!/usr/bin/env python3
"""从 protocol/*.yaml 生成 Python 布局常量和 C 固定 payload 宏。

直接生成 software/protocol/ 的 Python 模块及 firmware/protocol/ 的 C 头文件。
C 固定 payload 宏并入 cmd_list.h；动态 payload 和 Python 编解码由应用实现。
使用 --check 可检查当前生成文件是否与 YAML 一致。
"""
import argparse
from pathlib import Path

from schema import load_yaml, normalize

SRC_DIR = Path(__file__).resolve().parent
ROOT = SRC_DIR.parent
PY_OUT = ROOT / "software" / "protocol"
C_OUT = ROOT / "firmware" / "protocol"
NOTE = "# 此文件由 protocol/gen_protocol.py 自动生成，请勿手动修改"


def _hex(value):
    return f"0x{value:02X}"


def _render_class(name, entries, alias=True):
    lines = [f"class {name}:"]
    if not entries:
        lines.append("    pass")
    for member, value, comment in entries:
        line = f"    {member} = {value!r}"
        if comment:
            line += f"  # {comment}"
        lines.append(line)
    if alias and name.startswith("e") and len(name) > 1 and name[1].isupper():
        lines.extend(["", f"{name[1:]} = {name}"])
    return lines


def _render_init(types, params, obs, doc):
    lines = [NOTE, ""]
    lines.extend(_render_class(types["type_enum_name"], [
        (name, spec["value"], spec.get("c_type", ""))
        for name, spec in sorted(types["value_type"].items(), key=lambda pair: pair[1]["value"])
    ]))
    lines.append("")
    for class_name, fields, prefix, count_name in (
        ("Pidx", params["fields"], "PARAM", "PARAM_NUM"),
        ("Didx", obs["fields"], "DATA", "DATA_NUM"),
    ):
        entries = [(field["id_name"], index, field.get("name", ""))
                   for index, field in enumerate(fields, 1)]
        if prefix == "PARAM":
            entries.extend((item["id_name"], int(item["id"], 0), item.get("comment", ""))
                           for item in params.get("special_ids", []))
        entries.append((f"NUM_OF_{prefix}", len(fields) + 1, f"{prefix}总数"))
        lines.extend(_render_class(class_name, entries, alias=False))
        lines.append(f"{count_name} = {len(fields) + 1}")
        lines.append("")
    lines.extend(_render_class("CmdId", [
        (command["id_name"], command["id"], command.get("comment", ""))
        for command in doc["commands"]
    ], alias=False))
    lines.append("")
    for enum in doc.get("enums", []):
        lines.extend(_render_class(enum["enum_name"], [
            (item["id_name"], int(item["id"], 0), item.get("comment", ""))
            for item in enum["items"]
        ]))
        lines.append("")
    seen_options = {}
    for field in params["fields"] + obs["fields"]:
        option = field.get("options")
        if not option:
            continue
        name = option["enum_name"]
        if name in seen_options:
            if seen_options[name] != option:
                raise ValueError(f"选项枚举 {name} 定义不一致")
            continue
        seen_options[name] = option
        lines.extend(_render_class(name, [(item["id_name"], index, item.get("comment", ""))
                                          for index, item in enumerate(option["items"])]))
        lines.append("")
    lines.append("eCmdId = CmdId")
    lines.append("FbId = FeedBack")
    lines.append("")
    lines.extend([
        "class ProtocolField:",
        "    __slots__ = ('id', 'id_name', 'field', 'name', 'unit', 'val_type', 'default', 'options')",
        "    def __init__(self, id, id_name, field, name, unit, val_type, default=0, options=None):",
        "        self.id, self.id_name, self.field = id, id_name, field",
        "        self.name, self.unit, self.val_type = name, unit, val_type",
        "        self.default, self.options = default, options",
        "    def __repr__(self):",
        "        return f'ProtocolField({self.id_name})'",
        "",
    ])
    for variable, fields, index in (("PARAMETERS", params["fields"], "Pidx"), ("OBSDATA", obs["fields"], "Didx")):
        lines.append(f"{variable} = {{")
        for position, field in enumerate(fields, 1):
            options = field.get("options", {}).get("items", [])
            option_data = [(item.get("comment", ""), pos) for pos, item in enumerate(options)] or None
            lines.append("    %d: ProtocolField(%d, %r, %r, %r, %r, %r, %r, %r)," % (
                position, position, field["id_name"], field["field"], field.get("name", ""),
                field.get("unit", ""), field["val_type"], field.get("default", 0), option_data))
        lines.append("}")
        by_name = "PARAM_BY_NAME" if variable == "PARAMETERS" else "OBS_BY_NAME"
        lines.append(f"{by_name} = {{field.id_name: field for field in {variable}.values()}}")
        lines.append("")
    lines.extend([
        "COMMANDS = {name: {'id': value, 'comment': getattr(CmdId, name).__doc__ or ''} for name, value in vars(CmdId).items() if name.startswith('CMD_')}",
        "FEEDBACKS = {name: {'id': value, 'comment': '反馈'} for name, value in vars(FbId).items() if name.startswith('FB_')}",
        "FRAME = {name: {'id': value, 'comment': ''} for name, value in vars(UartFrame).items() if name.startswith('UART_')}",
        "CMD_TARGETS = {name: {'id': value, 'comment': ''} for name, value in vars(CmdTarget).items() if name.startswith('TARGET_')}"
        if any(enum["enum_name"] == "eCmdTarget" for enum in doc.get("enums", [])) else "CMD_TARGETS = {}",
        "MAX_PAYLOAD = %d" % doc["max_payload"],
        "SCHEMA_VERSION = %d" % doc["schema_version"],
    ])
    return "\n".join(lines) + "\n"


def _layout_commands(doc):
    """将条件请求展开为独立布局；主请求仅提供 selector offset。"""
    for cmd in doc["commands"]:
        yield cmd
        for variant, fields in cmd.get("request_variants", {}).items():
            yield {**cmd, "id_name": cmd["id_name"] + "_" + variant.upper(),
                   "base_name": cmd["id_name"], "variant_name": variant.upper(),
                   "request": fields, "response": None, "request_variants": {},
                   "variant_selector": cmd["request_selector"]}


def _payload_constants(doc, types, params, obs):
    """两端共用的纯布局常量；不生成编解码行为。"""
    yield "MAX_PAYLOAD", doc["max_payload"]
    for name, spec in types["value_type"].items():
        yield name + "_SIZE", spec["size"]
        if "format" in spec:
            yield name + "_FORMAT", "<" + spec["format"]
    tables = {"PARAMETERS": params["fields"], "OBSDATA": obs["fields"]}
    for cmd in _layout_commands(doc):
        for direction in ("request", "response"):
            fields = cmd[direction]
            if fields is None:
                continue
            prefix = cmd["id_name"] + "_" + direction.upper()
            fixed = max((f["offset"] + (f["size"] or 0) for f in fields), default=0)
            dynamic = next((f for f in fields if f["size"] is None), None)
            if direction == "request" and cmd.get("request_variants"):
                lengths = [max(f["offset"] + f["size"] for f in fields)
                           for fields in cmd["request_variants"].values()]
                yield prefix + "_MIN_SIZE", min(lengths)
                yield prefix + "_MAX_SIZE", max(lengths)
            elif dynamic is None:
                yield prefix + "_SIZE", fixed
            else:
                if dynamic["type"] == "DYNAMIC":
                    width = max(types["value_type"][f["val_type"]]["size"]
                                for f in tables[dynamic["value_source"]])
                    extra = width * dynamic.get("max_count", 1)
                elif dynamic.get("repeat"):
                    extra = types["value_type"][dynamic["type"]]["size"] * dynamic["max_count"]
                else:
                    extra = dynamic.get("max_size", doc["max_payload"] - fixed)
                yield prefix + "_FIXED_SIZE", fixed
                yield prefix + "_MAX_SIZE", min(doc["max_payload"], fixed + extra)
            for field in fields:
                key = prefix + "_" + field["name"].upper()
                yield key + "_OFFSET", field["offset"]
                if "bits" in field:
                    lo, hi = field["bits"]
                    yield key + "_SHIFT", lo
                    yield key + "_MASK", (1 << (hi - lo + 1)) - 1
                for prop in ("scale", "default", "max_count", "max_size"):
                    if prop in field:
                        yield key + "_" + prop.upper(), 2 ** field[prop] if prop == "scale" else field[prop]
                        if prop == "scale":
                            yield key + "_SCALE_EXP", field[prop]


def _render_payload(constants):
    return NOTE + "\n\n" + "\n".join(f"{name} = {value!r}" for name, value in constants) + "\n"


SCALE_NONE, SCALE_POW2, SCALE_SIGNED_EXP, SCALE_REF_U24, SCALE_REF_EXP = range(5)


def _descriptor(doc, types):
    """两端通用编解码引擎的唯一输入：字段池 + 布局表 + 变体表。

    缩放语义只在这里解释一次，引擎不再关心 YAML 写法：
    ``scale`` 为 2 的指数，``scale_exp`` 表示线上存负指数，
    ``scale_ref`` / ``scale_exp_ref`` 指向同一布局中前置的指数字段。
    """
    pool, layouts, variants = [], [], []

    def add_layout(cmd, direction, fields, detail, selector_value=None):
        start, index = len(pool), {}
        for field in fields:
            index[field["name"]] = len(pool) - start
            kind, exponent, ref = SCALE_NONE, 0, 0
            if "scale" in field:
                kind, exponent = SCALE_POW2, field["scale"]
            elif field.get("scale_exp"):
                kind = SCALE_SIGNED_EXP
            elif "scale_ref" in field:
                kind, ref = SCALE_REF_U24, index[field["scale_ref"]]
            elif "scale_exp_ref" in field:
                kind, ref = SCALE_REF_EXP, index[field["scale_exp_ref"]]
            lo, hi = field.get("bits", (0, 0))
            pool.append({
                "name": field["name"],
                "type": field["type"] if field["type"] in types["value_type"]
                        else "PROTO_TYPE_DYNAMIC",
                "offset": field["offset"], "size": field["size"] or 0,
                "lo": lo, "hi": hi, "kind": kind, "exponent": exponent, "ref": ref,
                "saturate": field.get("overflow") == "saturate",
            })
        sizes = [f["offset"] + (f["size"] or 0) for f in fields]
        layouts.append({
            "name": cmd["id_name"] + "_" + direction.upper()
                    + ("_" + detail.upper() if detail else ""),
            "macro": cmd["id_name"] + ("_" + detail.upper() if detail else "")
                     + "_" + direction.upper(),
            "cmd": cmd["id_name"], "id": cmd["id"], "direction": direction,
            "start": start, "count": len(fields), "fixed": max(sizes, default=0),
            "dynamic": any(f["size"] is None for f in fields),
        })
        return layouts[-1]["name"]

    for cmd in doc["commands"]:
        for direction in ("request", "response"):
            fields = cmd[direction]
            if not fields:
                continue
            base = add_layout(cmd, direction, fields, None)
            if direction != "request":
                continue
            for variant, variant_fields in cmd.get("request_variants", {}).items():
                mode = next(iter(variant_fields[0]["enum"].values()))
                name = add_layout(cmd, direction, variant_fields, variant)
                variants.append((base, mode, name))
    return pool, layouts, variants


def _render_descriptor_c(pool, layouts, variants):
    """生成描述表；不包含任何编解码逻辑，逻辑在 payload_codec.c。"""
    names = sorted({field["name"].upper() for field in pool})
    lines = ['#include "fieldvaluetype.h"', "", "#include <stdint.h>", "#include <stdbool.h>", ""]
    lines += [f'#define PROTO_STR_{name} "{name.lower()}"' for name in names]
    lines += ["", "#define PROTO_TYPE_DYNAMIC ((eFieldValueType)255)", "",
              "typedef enum", "{",
              "    PROTO_SCALE_NONE = 0,   /* 直接写原始值 */",
              "    PROTO_SCALE_POW2,       /* raw = round(value * 2^exponent) */",
              "    PROTO_SCALE_SIGNED_EXP, /* FLD_I4 存负指数：value = raw * 2^raw */",
              "    PROTO_SCALE_REF_U24,    /* 前置 U8 指数，无符号 24 位 */",
              "    PROTO_SCALE_REF_EXP,    /* 前置 FLD_I4 指数，有符号 16 位 */",
              "} eProtoScale;", "",
              "typedef struct", "{",
              "    const char *name;",
              "    eFieldValueType type;",
              "    uint8_t offset;",
              "    uint8_t size;   /* 0 表示由上下文决定 */",
              "    uint8_t lo;     /* FLD_I4 位域低位 */",
              "    uint8_t hi;     /* FLD_I4 位域高位；0 表示整字节 */",
              "    eProtoScale scale;",
              "    int8_t exponent; /* PROTO_SCALE_POW2 的指数 */",
              "    uint8_t ref;     /* 指数字段在所属布局内的下标 */",
              "    bool saturate;",
              "} tProtoField;", "",
              "typedef struct", "{",
              "    uint8_t cmd;       /* 命令 ID */",
              "    uint8_t direction; /* 0 请求，1 回复 */",
              "    uint16_t start;    /* 字段在 g_proto_fields 中的起点 */",
              "    uint8_t count;",
              "    uint8_t fixed;     /* 固定部分字节数 */",
              "    bool dynamic;",
              "} tProtoLayout;", "",
              "typedef struct", "{",
              "    const tProtoLayout *base;",
              "    uint8_t selector; /* 首字节模式值 */",
              "    const tProtoLayout *variant;",
              "} tProtoVariant;", "",
              "typedef enum", "{"]
    lines += [f"    PROTO_LAYOUT_{layout['name']}," for layout in layouts]
    lines += ["} eProtoLayout;", "", "static const tProtoField g_proto_fields[] = {"]
    for field in pool:
        bits = f"{field['lo']}U, {field['hi']}U" if field["hi"] else "0U, 0U"
        lines.append(
            f"    {{PROTO_STR_{field['name'].upper()}, {field['type']}, "
            f"{field['offset']}U, {field['size']}U, {bits}, (eProtoScale){field['kind']}, "
            f"{field['exponent']}, {field['ref']}U, {'true' if field['saturate'] else 'false'}}},")
    lines += ["};", "", "static const tProtoLayout g_proto_layouts[] = {"]
    for layout in layouts:
        lines.append(
            f"    [PROTO_LAYOUT_{layout['name']}] = {{{_hex(layout['id'])}, "
            f"{0 if layout['direction'] == 'request' else 1}U, {layout['start']}U, "
            f"{layout['count']}U, {layout['fixed']}U, "
            f"{'true' if layout['dynamic'] else 'false'}}},")
    lines += ["};", "", "static const tProtoVariant g_proto_variants[] = {"]
    lines += [f"    {{&g_proto_layouts[PROTO_LAYOUT_{base}], {mode}U, "
              f"&g_proto_layouts[PROTO_LAYOUT_{name}]}}," for base, mode, name in variants]
    if not variants:
        lines.append("    {NULL, 0U, NULL},")
    lines += ["};", "", f"#define PROTO_LAYOUT_COUNT {len(layouts)}U",
              f"#define PROTO_VARIANT_COUNT {len(variants)}U", ""]
    return lines


def _render_descriptor_py(pool, layouts, variants):
    """与 C 描述表同构的 Python 版本，供软件端通用引擎使用。"""
    lines = [NOTE, "",
             "SCALE_NONE = 0      # 直接写原始值",
             "SCALE_POW2 = 1      # raw = round(value * 2^exponent)",
             "SCALE_SIGNED_EXP = 2  # FLD_I4 存负指数：value = raw * 2^raw",
             "SCALE_REF_U24 = 3   # 前置 U8 指数，无符号 24 位",
             "SCALE_REF_EXP = 4   # 前置 FLD_I4 指数，有符号 16 位", "",
             "FIELDS = ["]
    for field in pool:
        lines.append(
            f"    dict(name={field['name']!r}, type={field['type']!r}, "
            f"offset={field['offset']}, size={field['size']}, lo={field['lo']}, "
            f"hi={field['hi']}, scale={field['kind']}, exponent={field['exponent']}, "
            f"ref={field['ref']}, saturate={field['saturate']!r}),")
    lines += ["]", "", "LAYOUTS = {"]
    for layout in layouts:
        lines.append(
            f"    {layout['name']!r}: dict(cmd={_hex(layout['id'])}, "
            f"direction={layout['direction']!r}, start={layout['start']}, "
            f"count={layout['count']}, fixed={layout['fixed']}, "
            f"dynamic={layout['dynamic']!r}),")
    lines += ["}", "", "VARIANTS = {"]
    grouped = {}
    for base, mode, name in variants:
        grouped.setdefault(base, []).append((mode, name))
    for base, entries in grouped.items():
        pairs = ", ".join(f"0x{mode:02X}: {name!r}" for mode, name in entries)
        lines.append(f"    {base!r}: {{{pairs}}},")
    lines += ["}", ""]
    return "\n".join(lines) + "\n"


def _macro(name, arguments, body):
    return [f"#define {name}({', '.join(arguments)}) \\",
            "    do { \\", *[f"        {line} \\" for line in body], "    } while (0)", ""]


def _payload_macros(doc, types):
    """宏只是描述表 + 通用引擎的薄封装，不生成任何编解码算法。

    缩放、位域、24 位符号扩展、变体分支全部由 payload_codec.h 处理，
    因此新增或修改字段只改 YAML，这里不需要跟着动。
    """
    lines = ["#include <stdint.h>", "#include <stdbool.h>", "#include <string.h>", "",
             '#include "descriptor.h"', '#include "payload_codec.h"', ""]
    lines += _macro("ENPACK_FEEDBACK", ["_out_length", "_packet", "_success"], [
        "((uint8_t *)(_packet))[0] = (_success) ? FB_EXECUTE : FB_FAILURE;",
        "(_out_length) = 1;",
    ])
    for command in _layout_commands(doc):
        for direction in ("request", "response"):
            fields = command[direction]
            if not fields or any(f["size"] is None for f in fields):
                continue
            if direction == "response" and command["feedback_response"]:
                continue
            packing = direction == "response"
            # 主请求宏输出模式值；分支宏的模式已由主请求解出，不再重复输出。
            selector = command.get("variant_selector")
            is_variant = bool(selector)
            has_length = packing or not is_variant
            internal = set() if packing else {
                field[ref] for field in fields for ref in ("scale_ref", "scale_exp_ref")
                if ref in field
            }
            index = {}
            for position, field in enumerate(fields):
                index[field["name"]] = position
            output = [field for field in fields
                      if (packing or field["name"] not in internal)
                      and not (is_variant and field["name"] == selector)]
            layout = command.get("base_name", command["id_name"]) + "_" + direction.upper()
            if command.get("variant_name"):
                layout += "_" + command["variant_name"]
            count = len(fields)
            for variant_fields in command.get("request_variants", {}).values():
                count = max(count, len(variant_fields))
            body = [f"double _proto_values[{count}];"]
            if packing:
                body += [f"_proto_values[{position}] = (double)({field['name']});"
                         for position, field in enumerate(fields)]
                body.append(f"(_out_length) = proto_encode(PROTO_LAYOUT_{layout}, (_packet), _proto_values);")
            elif has_length:
                body.append(f"(_out_length) = proto_decode(PROTO_LAYOUT_{layout}, (_packet), _proto_values);")
            else:
                # 分支宏不接收长度，探测结果只用于拒绝非法模式。
                body.append(f"if (proto_decode(PROTO_LAYOUT_{layout}, (_packet), _proto_values) == 0U)")
                body.append("    memset(_proto_values, 0, sizeof(_proto_values));")
            if not packing:
                body += [f"({field['name']}) = _proto_values[{index[field['name']]}];"
                         for field in output]
            prefix = "ENPACK" if packing else "DEPACK"
            macro_name = prefix + "_" + command["id_name"].removeprefix("CMD_") + "_" + direction.upper()
            arguments = (["_out_length"] if has_length else []) + ["_packet"]
            lines += _macro(macro_name, arguments + [field["name"] for field in output], body)
    return lines


def _c_enum(name, entries):
    lines = ["typedef enum", "{"]
    for member, value, comment in entries:
        lines.append(f"    {member} = {value}, /* {comment} */")
    return lines + [f"}} {name};", ""]


def _c_header(guard, lines):
    return ("/* 此文件由 protocol/gen_protocol.py 自动生成，请勿手动修改 */\n"
            f"#ifndef {guard}\n#define {guard}\n\n" + "\n".join(lines) + "\n#endif\n")


def _render_c_headers(types, params, obs, doc):
    value_types = types["value_type"]
    lines = ["#include <stdint.h>", "#include <stdbool.h>", ""]
    lines += _c_enum(types["type_enum_name"], [
        (name, spec["value"], spec["c_type"]) for name, spec in value_types.items()
    ])
    lines += [
        "/* 返回协议中的字节数，不是 C 存储类型的 sizeof；未知类型返回 0。 */",
        f"static inline uint8_t field_value_type_size({types['type_enum_name']} type)",
        "{",
        "    switch (type) {",
    ]
    for name, spec in value_types.items():
        lines.append(f"    case {name}: return {spec['size']}U;")
    lines += ["    default: return 0U;", "    }", "}", ""]
    outputs = {"fieldvaluetype.h": _c_header("PROTO_FIELDVALUETYPE_H", lines)}
    lines = _c_enum("eCmdId", [(cmd["id_name"], _hex(cmd["id"]), cmd.get("comment", ""))
                                   for cmd in doc["commands"]])
    for enum in doc.get("enums", []):
        lines += _c_enum(enum["enum_name"], [(item["id_name"], item["id"], item.get("comment", ""))
                                                for item in enum["items"]])
    lines += _payload_macros(doc, value_types)
    outputs["cmd_list.h"] = _c_header("PROTO_CMD_LIST_H", lines)
    for filename, data, prefix, pointers in (
        ("parameter_list.h", params, "PARAM", False),
        ("data_list.h", obs, "DATA", True),
    ):
        fields = data["fields"]
        lines = ['#include "fieldvaluetype.h"', ""]
        entries = [(field["id_name"], _hex(index), field.get("name", ""))
                   for index, field in enumerate(fields, 1)]
        entries.extend((item["id_name"], item["id"], item.get("comment", ""))
                       for item in data.get("special_ids", []))
        lines += _c_enum(data["enum_name"], entries)
        seen = set()
        for field in fields:
            option = field.get("options")
            if option and option["enum_name"] not in seen:
                seen.add(option["enum_name"])
                lines += _c_enum(option["enum_name"], [(item["id_name"], index, item.get("comment", ""))
                                                         for index, item in enumerate(option["items"])])
        lines += ["typedef struct", "{"]
        for field in fields:
            ctype = value_types[field["val_type"]]["c_type"]
            lines.append(f"    {ctype} {'*' if pointers else ''}{field['field']};")
        lines += [f"}} {data['struct_name']};", "", f"#define {prefix}_NUM {_hex(len(fields) + 1)}", ""]
        rows = []
        for field in fields:
            args = [field["id_name"], field["field"], field["val_type"]]
            if not pointers:
                default = field.get("default", 0)
                if field["val_type"] == "FLD_BOOL":
                    literal = "true" if default else "false"
                elif field["val_type"] == "FLD_F32":
                    literal = repr(float(default)) + "f"
                else:
                    literal = str(int(default)) + ("u" if not value_types[field["val_type"]]["signed"] else "")
                args.append(literal)
            rows.append("    X(" + ", ".join(args) + ")")
        lines.append(f"#define {prefix}_FIELDS(X) " + "\\\n" + " \\\n".join(rows))
        outputs[filename] = _c_header(f"PROTO_{prefix}_LIST_H", lines)
    return outputs


def _render_devices(doc):
    c_lines = ["#include <stdint.h>", "", "#define VERSION_ENCODE(major, minor, patch) " + chr(92),
               "    ((((uint32_t)(major) & 0xFFU) << 16) |  " + chr(92),
               "     (((uint32_t)(minor) & 0xFFU) << 8)  |  " + chr(92),
               "     ((uint32_t)(patch) & 0xFFU))", ""]
    py_lines = [NOTE, "", "def VERSION_ENCODE(major, minor, patch):",
                "    return ((major & 0xFF) << 16) | ((minor & 0xFF) << 8) | (patch & 0xFF)", ""]
    for series, modes in doc["devices"].items():
        if not series.isidentifier():
            raise ValueError(f"非法设备系列名: {series}")
        for mode in ("app", "bl"):
            for kind in ("vid", "pid"):
                raw = modes[mode][kind]
                value = int(raw, 0) if isinstance(raw, str) else raw
                if type(value) is not int or not 0 <= value <= 0xFFFF:
                    raise ValueError(f"非法 USB {kind}: {series}/{mode}: {raw}")
                name = f"{series}_{mode}_USB_{kind}".upper()
                c_lines.append(f"#define {name} 0x{value:04X}U")
                py_lines.append(f"{name} = 0x{value:04X}")
    return _c_header("PROTO_DEVICES_H", c_lines), "\n".join(py_lines) + "\n"


def _write(path, content, check):
    if check:
        if not path.exists() or path.read_text(encoding="utf-8") != content:
            raise SystemExit(f"生成文件不同步：{path.relative_to(ROOT)}")
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")
    print(f"[OK] {path}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="检查生成结果是否同步，不写文件")
    args = parser.parse_args()
    types = load_yaml(SRC_DIR / "fieldvaluetype.yaml")
    params = load_yaml(SRC_DIR / "parameter.yaml")
    obs = load_yaml(SRC_DIR / "obsdata.yaml")
    doc = normalize(load_yaml(SRC_DIR / "cmd.yaml"), types["value_type"], obs)
    pool, layouts, variants = _descriptor(doc, types)
    outputs = {
        PY_OUT / "__init__.py": _render_init(types, params, obs, doc),
        PY_OUT / "payload.py": _render_payload(list(_payload_constants(doc, types, params, obs))),
        PY_OUT / "descriptor.py": _render_descriptor_py(pool, layouts, variants),
        C_OUT / "descriptor.h": _c_header("PROTO_DESCRIPTOR_H",
                                          _render_descriptor_c(pool, layouts, variants)),
    }
    outputs.update({C_OUT / name: content
                    for name, content in _render_c_headers(types, params, obs, doc).items()})
    devices_h, devices_py = _render_devices(load_yaml(SRC_DIR / "devices.yaml"))
    outputs[C_OUT / "devices.h"] = devices_h
    outputs[PY_OUT / "devices.py"] = devices_py
    for path, content in outputs.items():
        _write(path, content, args.check)


if __name__ == "__main__":
    main()
