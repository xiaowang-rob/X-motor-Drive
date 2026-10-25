#!/usr/bin/env python3
"""从 protocol/*.json 生成 Python 布局常量和 C 固定 payload 宏。

直接生成 Software/protocol/ 的 Python 模块及 firmware/protocol/ 的 C 头文件。
C 固定 payload 宏并入 cmd_list.h；动态 payload 和 Python 编解码由应用实现。
使用 --check 可检查当前生成文件是否与 JSON 一致。
"""
import argparse
from pathlib import Path

from schema import load_json, normalize

SRC_DIR = Path(__file__).resolve().parent
ROOT = SRC_DIR.parent
PY_OUT = ROOT / "Software" / "protocol"
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


def _payload_constants(doc, types, params, obs):
    """两端共用的纯布局常量；不生成编解码行为。"""
    yield "MAX_PAYLOAD", doc["max_payload"]
    for name, spec in types["value_type"].items():
        yield name + "_SIZE", spec["size"]
        if "format" in spec:
            yield name + "_FORMAT", "<" + spec["format"]
    tables = {"PARAMETERS": params["fields"], "OBSDATA": obs["fields"]}
    for cmd in doc["commands"]:
        for direction in ("request", "response"):
            fields = cmd[direction]
            if fields is None:
                continue
            prefix = cmd["id_name"] + "_" + direction.upper()
            fixed = max((f["offset"] + (f["size"] or 0) for f in fields), default=0)
            dynamic = next((f for f in fields if f["size"] is None), None)
            if dynamic is None:
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
                        yield key + "_" + prop.upper(), field[prop]
        for name, raw in cmd.get("special_requests", {}).items():
            yield cmd["id_name"] + "_SPECIAL_" + name.upper(), bytes.fromhex(raw.removeprefix("0x"))


def _render_payload(constants):
    return NOTE + "\n\n" + "\n".join(f"{name} = {value!r}" for name, value in constants) + "\n"


def _macro(name, arguments, body):
    return [f"#define {name}({', '.join(arguments)}) \\",
            "    do { \\", *[f"        {line} \\" for line in body], "    } while (0)", ""]


def _payload_macros(doc, types):
    """仅生成固件所需的固定请求解包/固定回复装包；动态字段由业务处理。"""
    lines = ["#include <stdint.h>", "#include <stdbool.h>", "#include <string.h>", ""]
    lines += _macro("ENPACK_FEEDBACK", ["_out_length", "_packet", "_success"], [
        "((uint8_t *)(_packet))[0] = (_success) ? FB_EXECUTE : FB_FAILURE;",
        "(_out_length) = 1;",
    ])
    for command in doc["commands"]:
        for direction in ("request", "response"):
            fields = command[direction]
            if not fields or any(f["size"] is None for f in fields):
                continue
            if direction == "response" and command["feedback_response"]:
                continue
            packing = direction == "response"
            body = [f"{'uint8_t' if packing else 'const uint8_t'} *_proto_packet = (_packet);"]
            for field in fields:
                name, size, offset = field["name"], field["size"], field["offset"]
                typ = field["type"]
                raw = "_proto_" + name
                ctype = types[typ]["c_type"]
                if packing:
                    expression = f"({name})"
                    if "scale" in field:
                        # 无隐藏库依赖；整数缩放采用半远离零。
                        scaled = f"_proto_scaled_{name}"
                        body.append(f"double {scaled} = (double)({name}) * {field['scale']};")
                        expression = f"({scaled} >= 0 ? {scaled} + 0.5 : {scaled} - 0.5)"
                    body += [f"{ctype} {raw} = ({ctype}){expression};",
                             f"memcpy(_proto_packet + {offset}, &{raw}, {size});"]
                else:
                    if typ == "FLD_I4":
                        lo, hi = field["bits"]
                        mask = (1 << (hi - lo + 1)) - 1
                        body += [f"int8_t {raw} = (_proto_packet[{offset}] >> {lo}) & {mask};",
                                 f"if ({raw} & 8) {raw} -= 16;"]
                    elif typ == "FLD_I24":
                        body += [f"int32_t {raw} = (int32_t)((uint32_t)_proto_packet[{offset}] |",
                                 f"    ((uint32_t)_proto_packet[{offset + 1}] << 8) | ((uint32_t)_proto_packet[{offset + 2}] << 16));",
                                 f"if ({raw} & 0x800000) {raw} -= 0x1000000;"]
                    else:
                        body += [f"{ctype} {raw};", f"memcpy(&{raw}, _proto_packet + {offset}, {size});"]
            if not packing:
                for field in fields:
                    name = field["name"]
                    expression = "_proto_" + name
                    if field.get("scale_exp"):
                        expression = "-" + expression
                    elif "scale" in field:
                        expression = f"(float){expression} / {field['scale']}"
                    elif "scale_exp_ref" in field:
                        exponent = "_proto_" + field["scale_exp_ref"]
                        # 直接使用线上的有符号指数，避免依赖调用者输出变量类型。
                        expression = (f"({exponent} <= 0 ? (float){expression} / (1u << -{exponent})"
                                      f" : (float){expression} * (1u << {exponent}))")
                    body.append(f"({name}) = {expression};")
            length = max(f["offset"] + f["size"] for f in fields)
            body.append(f"(_out_length) = {length};")
            prefix = "ENPACK" if packing else "DEPACK"
            macro_name = prefix + "_" + command["id_name"].removeprefix("CMD_") + "_" + direction.upper()
            lines += _macro(macro_name, ["_out_length", "_packet", *[f["name"] for f in fields]], body)
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
        lines += _c_enum(data["enum_name"], [(field["id_name"], _hex(index), field.get("name", ""))
                                                for index, field in enumerate(fields, 1)])
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
    types = load_json(SRC_DIR / "fieldvaluetype.json")
    params = load_json(SRC_DIR / "parameter.json")
    obs = load_json(SRC_DIR / "obsdata.json")
    doc = normalize(load_json(SRC_DIR / "cmd.json"), types["value_type"])
    outputs = {
        PY_OUT / "__init__.py": _render_init(types, params, obs, doc),
        PY_OUT / "payload.py": _render_payload(list(_payload_constants(doc, types, params, obs))),
    }
    outputs.update({C_OUT / name: content
                    for name, content in _render_c_headers(types, params, obs, doc).items()})
    for path, content in outputs.items():
        _write(path, content, args.check)


if __name__ == "__main__":
    main()
