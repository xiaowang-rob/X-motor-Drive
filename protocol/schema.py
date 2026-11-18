"""加载协议 YAML，仅规范化生成代码使用的命令和枚举元数据。"""
import re
from pathlib import Path

import yaml

_IDENTIFIER = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


def load_yaml(path):
    return yaml.safe_load(Path(path).read_text(encoding="utf-8"))


def _identifier(name):
    return isinstance(name, str) and _IDENTIFIER.fullmatch(name)


def _u8(value):
    number = int(value, 0) if isinstance(value, str) else value
    if type(number) is not int or not 0 <= number <= 0xFF:
        raise ValueError(f"ID 必须为 U8：{value!r}")
    return number


def normalize(cmd_doc):
    """只读取命令 ID、注释及枚举；request/response 等 payload 描述不参与处理。"""
    commands = []
    seen_names, seen_ids = set(), set()
    for source in cmd_doc.get("commands", []):
        name, ident = source.get("id_name"), _u8(source.get("id"))
        if not _identifier(name) or not name.startswith("CMD_"):
            raise ValueError(f"无效命令名：{name!r}")
        if name in seen_names or ident in seen_ids:
            raise ValueError(f"命令名称或 ID 重复：{name}")
        seen_names.add(name)
        seen_ids.add(ident)
        commands.append({"id_name": name, "id": ident,
                         "comment": source.get("comment", "")})
    if not commands:
        raise ValueError("commands 不能为空")

    enums = []
    for source in cmd_doc.get("enums", []):
        name = source.get("enum_name")
        if not _identifier(name) or name in seen_names:
            raise ValueError(f"枚举名称无效或重复：{name!r}")
        seen_names.add(name)
        items = []
        for item in source["items"]:
            member, ident = item.get("id_name"), _u8(item.get("id"))
            if not _identifier(member) or member in seen_names:
                raise ValueError(f"枚举成员名称无效或重复：{member!r}")
            seen_names.add(member)
            items.append({"id_name": member, "id": f"0x{ident:02X}",
                          "comment": item.get("comment", "")})
        enums.append({"enum_name": name, "items": items})
    return {"commands": commands, "enums": enums,
            "max_payload": cmd_doc["max_payload"],
            "schema_version": cmd_doc["schema_version"]}
