#!/usr/bin/env python3
"""
从 usr_config.json 生成 C 和 Python 的共享常量定义。
用法: python3 codegen.py <json_path> <c_output> <py_output>

特性：
- C 宏定义三列对齐 (#define NAME  VALUE  /* comment */)
- C 枚举成员名称/值/注释对齐
- Python 类成员等号对齐，带注释
"""

import json
import sys
from pathlib import Path


def load_protocol(json_path):
    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)
    return data["protocol"]


# ─── C 代码生成 ────────────────────────────────────────────
def gen_c_header(proto, output_path):
    lines = [
        "/* ===== 此文件由 build.py 自动生成，请勿手动修改 ，相关配置在 usr_config.json 中 ===== */",
        "#ifndef __PROTOCOL_DEFS_H",
        "#define __PROTOCOL_DEFS_H",
        "",
    ]

    # ===== 辅助：生成对齐的枚举 =====
    def aligned_enum(enum_name, items, prefix="", end_name=None):
        """
        生成一个枚举，并对齐名称列和值列。
        items: 列表，每个元素是 {"name": str, "value": int|None, "comment": str}
        prefix: 加到每个名称前面的字符串（如 'STREAM_'）
        end_name: 可选，末尾额外添加的成员（如 COUNT_PARAM）
        """
        # 预处理实际要输出的项
        entries = []
        idx = 0
        for it in items:
            val = it.get("value")
            if val is not None:
                idx = int(val, 0)  # 支持 0x 等
            name = prefix + it["name"]
            comment = it.get("comment", "")
            entries.append((name, idx, comment))
            idx += 1

        # 计算名称列最大宽度（至少 "    " 缩进后的名称长度）
        max_name_len = max((len(name) for name, _, _ in entries), default=0)
        # 值列宽度固定，因为值都是数字，对齐到最长可能值的长度
        max_val_len = max((len(str(val)) for _, val, _ in entries), default=0)
        # 默认缩进4空格
        indent = "    "

        result = [f"typedef enum {{"]
        for name, val, comment in entries:
            val_str = str(val)
            # 名称: {name:{max_name_len}}  , 值: {val_str:{max_val_len}}, 注释
            if comment:
                comment_str = f"  /* {comment} */"
            else:
                comment_str = ""
            line = f"{indent}{name:<{max_name_len}} = {val_str:>{max_val_len}},{comment_str}"
            result.append(line)
        if end_name:
            # end_name 视为额外成员，通常无注释
            end_line = f"{indent}{end_name:<{max_name_len}} = {idx}"
            result.append(end_line)
        result.append(f"}} {enum_name};\n")
        return result

    # 1. eParameter
    lines.extend(
        aligned_enum("eParameter", proto["parameter_index"],end_name="PARAM_NUM")
    )


    # 2. eData_stream
    lines.extend(aligned_enum("eData_stream", proto["data_stream"], end_name="DATA_NUM"))


    # 3. other_strings 中的枚举 (eSensorMode, eRunMode, ...)
    if "other_strings" in proto:
        for key, entry in proto["other_strings"].items():
            enum_name = entry["enum_name"]
            items = entry["items"]
            lines.extend(aligned_enum(enum_name, items))
            lines.append("")

    # ===== 宏定义（三列对齐） =====
    all_macros = []  # (name, value, comment)
    # 命令 ID
    for cmd in proto["cmd_ids"]:
        comment = cmd.get("comment", "")
        all_macros.append((cmd["name"], cmd["value"], comment))
    # 反馈 ID
    for fb in proto["feedback_ids"]:
        comment = fb.get("comment", "")
        all_macros.append((fb["name"], fb["value"], comment))
    # USB 格式
    for key, cfg in proto["usb_format"].items():
        comment = cfg.get("comment", "")
        all_macros.append((key, cfg["value"], comment))
    # UART 格式
    for key, cfg in proto["uart_format"].items():
        comment = cfg.get("comment", "")
        all_macros.append((key, cfg["value"], comment))
    # 帧长度
    for key, cfg in proto["frame_length"].items():
        all_macros.append((key, str(cfg["value"]), ""))

    # 计算列宽（名称、值）
    max_name_len = max((len(name) for name, _, _ in all_macros), default=0)
    max_val_len = max((len(val) for _, val, _ in all_macros), default=0)

    # 输出分组
    sections = [
        (
            "/* ---------- CMD ID ---------- */",
            [
                (cmd["name"], cmd["value"], cmd.get("comment", ""))
                for cmd in proto["cmd_ids"]
            ],
        ),
        (
            "/* ---------- 反馈 ID ---------- */",
            [
                (fb["name"], fb["value"], fb.get("comment", ""))
                for fb in proto["feedback_ids"]
            ],
        ),
        (
            "/* ---------- USB 协议格式 ---------- */",
            [
                (key, cfg["value"], cfg.get("comment", ""))
                for key, cfg in proto["usb_format"].items()
            ],
        ),
        (
            "/* ---------- UART 协议格式 ---------- */",
            [
                (key, cfg["value"], cfg.get("comment", ""))
                for key, cfg in proto["uart_format"].items()
            ],
        ),
        (
            "/* ---------- 帧长度 ---------- */",
            [
                (key, str(cfg["value"]), "")
                for key, cfg in proto["frame_length"].items()
            ],
        ),
    ]

    for header, group in sections:
        lines.append(header)
        for name, value, comment in group:
            comment_str = f"  /* {comment} */" if comment else ""
            # 对齐：#define NAME  VALUE  comment
            line = f"#define {name:<{max_name_len}}  {value:<{max_val_len}}  {comment_str}".rstrip()
            lines.append(line)
        lines.append("")

    lines.append("#endif")
    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"[OK] 生成 C 头文件: {output_path}")


# ─── Python 代码生成 ────────────────────────────────────────
def gen_python_module(proto, output_path):
    lines = [
        "# 此文件由 codegen.py 自动生成，请勿手动修改，相关配置在 usr_config.json 中",
        "",
    ]

    # 辅助：生成对齐的类
    def aligned_class(class_name, items, is_index=True, extra=None):
        """
        items: 列表，每个元素 {name, value, comment}
        is_index: 若为True，value=None 表示索引自动递增；否则使用固定值
        extra: 可选 (name, value, comment) 加到末尾
        """
        result = [f"class {class_name}:"]
        # 收集所有成员 (name, value, comment)
        members = []
        idx = 0
        for it in items:
            val = it.get("value")
            if is_index:
                if val is not None:
                    idx = int(val, 0)
                members.append((it["name"], idx, it.get("comment", "")))
                idx += 1
            else:  # 固定值
                members.append((it["name"], it["value"], it.get("comment", "")))
        if extra:
            members.append(extra)

        # 计算名称最大宽度
        max_len = max((len(name) for name, _, _ in members), default=0)
        for name, value, comment in members:
            comment_str = f"  # {comment}" if comment else ""
            # 对齐：    NAME       = VALUE  # comment
            line = f"    {name:<{max_len}} = {value}{comment_str}"
            result.append(line)
        result.append("")
        return result

    # Pidx (参数索引)
    lines.extend(
        aligned_class(
            "Pidx",
            proto["parameter_index"],
            is_index=True,
            extra=("NUM_OF_PARAM", len(proto["parameter_index"]), "参数总数"),
        )
    )
    # Lidx (日志索引，仅 Python)
    if "log_index" in proto:
        lines.extend(
            aligned_class(
                "Lidx",
                proto["log_index"],
                is_index=True,
                extra=("log_num", len(proto["log_index"]), "日志字段数量"),
            )
        )
    # Didx (数据流索引)
    lines.extend(aligned_class("Didx", proto["data_stream"], is_index=True))
    # Cidx (命令 ID)
    lines.extend(aligned_class("Cidx", proto["cmd_ids"], is_index=False))
    # Fidx (反馈 ID)
    lines.extend(aligned_class("Fidx", proto["feedback_ids"], is_index=False))

    # Pkt (协议格式常量)
    format_items = []
    for key, cfg in proto['usb_format'].items():
        format_items.append((key, cfg['value'], cfg.get('comment', '')))
    for key, cfg in proto['uart_format'].items():
        format_items.append((key, cfg['value'], cfg.get('comment', '')))
    for key, cfg in proto['frame_length'].items():
        format_items.append((key, cfg['value'], ''))

    if format_items:
        lines.append('class Pkt:')
        max_len = max(len(name) for name, _, _ in format_items)
        for name, value, comment in format_items:
            comment_str = f'  # {comment}' if comment else ''
            lines.append(f'    {name:<{max_len}} = {value}{comment_str}')
        lines.append('')

    # Sidx (状态包索引，仅 Python)
    if "status_index" in proto:
        lines.extend(aligned_class("Sidx", proto["status_index"], is_index=True))
    # Midx (模式字符串列表)
    if "other_strings" in proto:
        lines.append("class Midx:")
        # 处理普通模式：提取注释列表作为字符串数组
        for key, entry in proto["other_strings"].items():
            strings = [m["comment"] for m in entry["items"]]
            # 对齐类成员
            lines.append(
                f"    {key:<{len(key)}} = {json.dumps(strings, ensure_ascii=False)}"
            )
        # 动态生成 data_select
        didx_comments = [it.get("comment", it["name"]) for it in proto["data_stream"]]
        data_select = ["NONE"] + didx_comments
        lines.append(f"    data_select = {json.dumps(data_select, ensure_ascii=False)}")
        lines.append("")

    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"[OK] 生成 Python 模块: {output_path}")


# ─── 主入口 ────────────────────────────────────────────────
def main():
    if len(sys.argv) != 4:
        print("用法: python3 codegen.py <json_path> <c_output> <py_output>")
        sys.exit(1)
    json_path = Path(sys.argv[1])
    c_out = Path(sys.argv[2])
    py_out = Path(sys.argv[3])
    proto = load_protocol(json_path)
    gen_c_header(proto, c_out)
    gen_python_module(proto, py_out)


if __name__ == "__main__":
    main()
