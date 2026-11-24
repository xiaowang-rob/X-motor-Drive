#!/usr/bin/env python3
"""从 protocol/*.yaml 生成命令枚举、参数/观测元数据和设备常量。

payload 描述仅供查阅，不参与生成或校验。
固件 payload 宏手工维护于 firmware/app/payload.h，
上位机编解码手工维护于 software/app/services/payload_codec.py。
使用 --check 检查生成文件是否与 YAML 元数据一致。
"""
import argparse
from pathlib import Path

from schema import load_yaml, normalize

SRC_DIR = Path(__file__).resolve().parent
ROOT = SRC_DIR.parent
PY_OUT = ROOT / "xdr-tool" / "protocol"
C_OUT = ROOT / "xdr-s"/ "MOTOR_DRIVE" / "protocol"
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
    doc = normalize(load_yaml(SRC_DIR / "cmd.yaml"))
    outputs = {
        PY_OUT / "__init__.py": _render_init(types, params, obs, doc),
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
