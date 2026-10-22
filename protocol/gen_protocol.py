#!/usr/bin/env python3
"""
由 protocol/*.json 生成固件 C 头文件与上位机 Python 协议包。

输入：
  fieldvaluetype.json  字段类型定义
  parameter.json       参数定义
  obsdata.json         观测数据定义
  cmd.json             命令 / 反馈 / 帧格式枚举

输出：
  firmware/protocol/fieldvaluetype.h
  firmware/protocol/parameter_list.h
  firmware/protocol/data_list.h
  firmware/protocol/cmd_list.h
  Software/protocol/__init__.py

parameter.json / obsdata.json 中的字段按数组顺序自动分配 ID（从 0x01 开始），
JSON 里不再需要写 id；字段上的 "options" 直接生成在对应列表头文件内。

用法：
  python3 protocol/gen_protocol.py
"""

import json
from pathlib import Path

SRC_DIR = Path(__file__).resolve().parent
ROOT = SRC_DIR.parent
OUT_DIR = ROOT / "firmware" / "protocol"
PY_OUT = ROOT / "Software" / "protocol" / "__init__.py"
GEN_NOTE = f"/* 此文件由 {SRC_DIR.name}/{Path(__file__).name} 自动生成，请勿手动修改 */"
PY_NOTE = f"# 此文件由 {SRC_DIR.name}/{Path(__file__).name} 自动生成，请勿手动修改"

# 两个列表头文件之间的差异集中在这里
LIST_CONFIGS = (
    {
        "src": "parameter.json",
        "dst": "parameter_list.h",
        "num": "PARAM_NUM",
        "fields_macro": "PARAM_FIELDS",
        "pointer": False,
        "id_title": "参数字段 ID 枚举",
        "option_title": "参数选项",
    },
    {
        "src": "obsdata.json",
        "dst": "data_list.h",
        "num": "DATA_NUM",
        "fields_macro": "DATA_FIELDS",
        "pointer": True,
        "id_title": "观测变量 ID 枚举",
        "option_title": "观测选项",
    },
)


def load_json(name):
    return json.loads((SRC_DIR / name).read_text(encoding="utf-8"))


def hex_id(value, max_value):
    """按最大 ID 自动选择十六进制宽度。"""
    width = max(2, len(f"{max_value:X}"))
    return f"0x{value:0{width}X}"


def render_enum(enum_name, entries, comments=()):
    """
    生成枚举：名称/值不补空格，行尾注释统一对齐。

    entries: [(name, value, comment), ...]
    """
    if not entries:
        raise ValueError(f"枚举 {enum_name} 没有成员")

    codes = [f"    {name} = {value}," for name, value, _ in entries]
    max_code = max(len(code) for code in codes)

    lines = [f"// {comment}" for comment in comments if comment]
    lines.extend(["typedef enum", "{"])
    for index, (name, value, comment) in enumerate(entries):
        code = f"    {name} = {value}"
        if index < len(entries) - 1:
            code += ","
        line = f"{code:<{max_code}} // {comment}" if comment else code
        lines.append(line)
    lines.append(f"}} {enum_name};")
    return lines


def render_struct(struct_name, rows):
    """rows: [(声明, 注释), ...]"""
    if not rows:
        raise ValueError(f"结构体 {struct_name} 没有字段")

    codes = [f"    {decl}" for decl, _ in rows]
    max_code = max(len(code) for code in codes)

    lines = ["typedef struct", "{"]
    for code, (_, comment) in zip(codes, rows):
        lines.append(f"{code:<{max_code}} // {comment}" if comment else code)
    lines.append(f"}} {struct_name};")
    return lines


def render_xmacro(macro_name, rows):
    """生成反斜杠对齐的 X-macro。"""
    if not rows:
        raise ValueError(f"X-macro {macro_name} 没有成员")

    body = [f"    {row}" for row in rows]
    width = max(len(line) for line in body)

    lines = [f"#define {macro_name}(X)".ljust(width) + " \\"]
    for index, line in enumerate(body):
        if index < len(body) - 1:
            line = line.ljust(width) + " \\"
        lines.append(line)
    return lines


def wrap_header(dst, body, includes=()):
    guard = f"__{dst.stem.upper()}_H"
    lines = [GEN_NOTE, "", f"#ifndef {guard}", f"#define {guard}", ""]
    lines.extend(f"#include {include}" for include in includes)
    if includes:
        lines.append("")
    lines.extend(body)
    lines.extend(["", "#endif"])

    dst.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"[OK] {dst.relative_to(ROOT)}")


def default_literal(field):
    """PARAM_FIELDS 中使用的 C 默认值字面量。"""
    val_type = field["val_type"]
    default = field.get("default", 0)

    if val_type == "FLD_BOOL":
        return "true" if default else "false"
    if val_type in ("FLD_U8", "FLD_U16", "FLD_U32"):
        return f"{int(default)}u"
    if val_type == "FLD_F32":
        return f"{float(default)!r}f"
    raise ValueError(f"未知类型 {val_type}（字段 {field['field']}）")


def collect_options(fields):
    """按字段顺序收集选项枚举，并按 enum_name 去重。"""
    options = {}
    for field in fields:
        option = field.get("options")
        if not option:
            continue
        name = option["enum_name"]
        if name in options and options[name] != option:
            raise ValueError(f"选项枚举 {name} 的定义不一致")
        options.setdefault(name, option)
    return list(options.values())


def gen_fieldvaluetype(doc):
    entries = [
        (name, str(cfg["value"]), cfg.get("c_type", ""))
        for name, cfg in sorted(
            doc["value_type"].items(), key=lambda item: item[1]["value"]
        )
    ]
    body = render_enum(doc["type_enum_name"], entries)
    wrap_header(
        OUT_DIR / "fieldvaluetype.h",
        body,
        includes=("<stdint.h>", "<stdbool.h>"),
    )


def gen_list(cfg, doc, value_type):
    fields = doc["fields"]
    count = len(fields) + 1  # ID 从 1 开始，最大 ID = len(fields)
    body = []

    body.extend(
        render_enum(
            doc["enum_name"],
            [
                (field["id_name"], hex_id(index, count), field.get("name", ""))
                for index, field in enumerate(fields, 1)
            ],
            [cfg["id_title"]],
        )
    )
    body.append("")

    options = collect_options(fields)
    if options:
        body.append(f"// ---------- {cfg['option_title']} ----------")
        for index, option in enumerate(options):
            if index:
                body.append("")
            body.extend(
                render_enum(
                    option["enum_name"],
                    [
                        (item["id_name"], str(item_index), item.get("comment", ""))
                        for item_index, item in enumerate(option["items"])
                    ],
                    [option.get("comment", "")],
                )
            )
        body.append("")

    struct_rows = []
    macro_rows = []
    for field in fields:
        c_type = value_type[field["val_type"]]["c_type"]
        comment = field.get("name", "")
        if cfg["pointer"]:
            struct_rows.append((f"{c_type} *{field['field']};", comment))
            macro_rows.append(
                f"X({field['id_name']}, {field['field']}, {field['val_type']})"
            )
        else:
            struct_rows.append((f"{c_type} {field['field']};", comment))
            macro_rows.append(
                f"X({field['id_name']}, {field['field']}, "
                f"{field['val_type']}, {default_literal(field)})"
            )

    body.extend(render_struct(doc["struct_name"], struct_rows))
    body.append("")
    body.append(f"#define {cfg['num']} {hex_id(count, count)}")
    body.append("")
    body.extend(render_xmacro(cfg["fields_macro"], macro_rows))

    wrap_header(OUT_DIR / cfg["dst"], body, includes=('"fieldvaluetype.h"',))


def gen_cmd(cmd_defs):
    body = []
    for index, cmd in enumerate(cmd_defs):
        if index:
            body.append("")
        body.extend(
            render_enum(
                cmd["enum_name"],
                [
                    (item["id_name"], item["id"], item.get("comment", ""))
                    for item in cmd["items"]
                ],
                [cmd.get("comment", "")],
            )
        )
    wrap_header(OUT_DIR / "cmd_list.h", body)


# ─── Python 协议包生成 ──────────────────────────────────────
def py_literal(value):
    return repr(value)


def py_options(field):
    option = field.get("options")
    if not option:
        return "None"
    items = ", ".join(
        f"({py_literal(item.get('comment', ''))}, {index})"
        for index, item in enumerate(option["items"])
    )
    return f"[{items}]"


def render_py_class(class_name, entries, comments=()):
    lines = [f"# {comment}" for comment in comments if comment]
    lines.append(f"class {class_name}:")
    if not entries:
        lines.append("    pass")
    for name, value, comment in entries:
        line = f"    {name} = {value}"
        if comment:
            line += f"  # {comment}"
        lines.append(line)

    # eCmdId -> CmdId / eMotorList -> MotorList，方便 Python 侧使用
    if len(class_name) > 1 and class_name[0] == "e" and class_name[1].isupper():
        lines.extend(["", f"{class_name[1:]} = {class_name}"])
    return lines


def render_py_field(field, const):
    values = [
        const,
        py_literal(field["id_name"]),
        py_literal(field["field"]),
        py_literal(field.get("name", "")),
        py_literal(field.get("unit", "")),
        py_literal(field["val_type"]),
        py_literal(field.get("default", 0)),
        py_options(field),
    ]
    return f"    {const}: ProtocolField(" + ", ".join(values) + "),"


def render_py_meta(name, cmd):
    """生成命令/反馈/帧格式/目标的 id -> {id, comment} 元数据。"""
    lines = [f"{name} = {{"]
    for item in cmd["items"]:
        item_id = item["id"]
        comment = py_literal(item.get("comment", ""))
        lines.append(
            f"    {py_literal(item['id_name'])}: "
            f'{{"id": {item_id}, "comment": {comment}}},'
        )
    lines.append("}")
    return lines


def gen_python(fieldvaluetype, param_doc, data_doc, cmd_defs):
    param_fields = param_doc["fields"]
    data_fields = data_doc["fields"]
    param_count = len(param_fields) + 1
    data_count = len(data_fields) + 1

    lines = [PY_NOTE, ""]

    type_entries = [
        (name, str(cfg["value"]), cfg.get("c_type", ""))
        for name, cfg in sorted(
            fieldvaluetype["value_type"].items(), key=lambda item: item[1]["value"]
        )
    ]
    lines.extend(render_py_class(fieldvaluetype["type_enum_name"], type_entries))
    lines.append("")

    lines.extend(
        render_py_class(
            "Pidx",
            [
                (field["id_name"], hex_id(index, param_count), field.get("name", ""))
                for index, field in enumerate(param_fields, 1)
            ]
            + [("NUM_OF_PARAM", hex_id(param_count, param_count), "参数总数")],
        )
    )
    lines.append(f"PARAM_NUM = {hex_id(param_count, param_count)}")
    lines.append("")

    lines.extend(
        render_py_class(
            "Didx",
            [
                (field["id_name"], hex_id(index, data_count), field.get("name", ""))
                for index, field in enumerate(data_fields, 1)
            ]
            + [("NUM_OF_DATA", hex_id(data_count, data_count), "观测字段总数")],
        )
    )
    lines.append(f"DATA_NUM = {hex_id(data_count, data_count)}")
    lines.append("")

    for cmd in cmd_defs:
        lines.extend(
            render_py_class(
                cmd["enum_name"],
                [
                    (item["id_name"], item["id"], item.get("comment", ""))
                    for item in cmd["items"]
                ],
                [cmd.get("comment", "")],
            )
        )
        lines.append("")

    lines.append("# 命令 / 反馈 / 帧格式 / 控制目标元数据")
    for meta_name, enum_name in (
        ("COMMANDS", "eCmdId"),
        ("FEEDBACKS", "eFbId"),
        ("FRAME", "eUartFrame"),
        ("CMD_TARGETS", "eCmdTarget"),
    ):
        cmd = next((item for item in cmd_defs if item["enum_name"] == enum_name), None)
        if cmd:
            lines.extend(render_py_meta(meta_name, cmd))
            lines.append("")

    seen_options = set()
    for option in collect_options(param_fields) + collect_options(data_fields):
        if option["enum_name"] in seen_options:
            continue
        seen_options.add(option["enum_name"])
        lines.extend(
            render_py_class(
                option["enum_name"],
                [
                    (item["id_name"], str(index), item.get("comment", ""))
                    for index, item in enumerate(option["items"])
                ],
                [option.get("comment", "")],
            )
        )
        lines.append("")

    lines.extend(
        [
            "class ProtocolField:",
            '    __slots__ = ("id", "id_name", "field", "name", "unit", "val_type", "default", "options")',
            "",
            "    def __init__(self, id, id_name, field, name, unit, val_type, default=0, options=None):",
            "        self.id = id",
            "        self.id_name = id_name",
            "        self.field = field",
            "        self.name = name",
            "        self.unit = unit",
            "        self.val_type = val_type",
            "        self.default = default",
            "        self.options = options",
            "",
            "    def __repr__(self):",
            '        return f"ProtocolField({self.id_name})"',
            "",
            "PARAMETERS = {",
        ]
    )
    lines.extend(
        render_py_field(field, f"Pidx.{field['id_name']}") for field in param_fields
    )
    lines.extend(
        [
            "}",
            "PARAM_BY_NAME = {field.id_name: field for field in PARAMETERS.values()}",
            "",
            "OBSDATA = {",
        ]
    )
    lines.extend(
        render_py_field(field, f"Didx.{field['id_name']}") for field in data_fields
    )
    lines.extend(
        [
            "}",
            "OBS_BY_NAME = {field.id_name: field for field in OBSDATA.values()}",
        ]
    )

    PY_OUT.parent.mkdir(parents=True, exist_ok=True)
    PY_OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"[OK] {PY_OUT.relative_to(ROOT)}")


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    fieldvaluetype = load_json("fieldvaluetype.json")
    param_doc = load_json("parameter.json")
    data_doc = load_json("obsdata.json")
    cmd_defs = load_json("cmd.json")

    gen_fieldvaluetype(fieldvaluetype)
    for cfg, doc in zip(LIST_CONFIGS, (param_doc, data_doc)):
        gen_list(cfg, doc, fieldvaluetype["value_type"])
    gen_cmd(cmd_defs)
    gen_python(fieldvaluetype, param_doc, data_doc, cmd_defs)


if __name__ == "__main__":
    main()
