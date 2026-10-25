"""协议 JSON 的加载、规范化和校验。"""
import copy
import json
import re
from pathlib import Path

_IDENTIFIER = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
_DYNAMIC = {"DYNAMIC", "BYTES", "STRING"}


def load_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def normalize(cmd_doc, value_types):
    """返回不含快捷写法、具备 offset/size 的不可变协议模型。"""
    if cmd_doc.get("schema_version") != 1:
        raise ValueError("cmd.json schema_version 必须为 1")
    if cmd_doc.get("endian") != "little":
        raise ValueError("当前只支持 little endian")
    maximum = cmd_doc.get("max_payload")
    if not isinstance(maximum, int) or not 1 <= maximum <= 255:
        raise ValueError("max_payload 必须是 1..255 的整数")

    sizes = {name: spec["size"] for name, spec in value_types.items()}
    enums = {}
    for enum in cmd_doc.get("enums", []):
        values = {item["id_name"]: int(item["id"], 0) if isinstance(item["id"], str)
                  else item["id"] for item in enum["items"]}
        enums[enum["enum_name"]] = values
    feedback_values = enums.get("eFeedBack", {})
    if set(feedback_values) != {"FB_EXECUTE", "FB_FAILURE"} or any(
            not isinstance(value, int) or not 0 <= value <= 255 for value in feedback_values.values()):
        raise ValueError("enums.eFeedBack 必须定义 U8 的 FB_EXECUTE/FB_FAILURE")
    feedback = {"type": "FLD_U8", "enum": feedback_values}

    commands = []
    seen_names, seen_ids = set(), set()
    for source in cmd_doc.get("commands", []):
        command = copy.deepcopy(source)
        command["feedback_response"] = command.get("response") == "feedback"
        name, ident = command.get("id_name"), command.get("id")
        if not isinstance(name, str) or not _IDENTIFIER.fullmatch(name) or not name.startswith("CMD_"):
            raise ValueError(f"无效命令名：{name!r}")
        if name in seen_names or not isinstance(ident, str):
            raise ValueError(f"重复命令名或无效 ID：{name}")
        try:
            command["id"] = int(ident, 0)
        except ValueError as error:
            raise ValueError(f"命令 {name} 的 id 无效") from error
        if not 0 <= command["id"] <= 0xFF or command["id"] in seen_ids:
            raise ValueError(f"命令 ID 重复或越界：{name}")
        seen_names.add(name)
        seen_ids.add(command["id"])
        for direction in ("request", "response"):
            value = command.get(direction)
            if direction == "response" and value == "none":
                command[direction] = None
                continue
            if direction == "response" and value == "feedback":
                command[direction] = [{"name": "status", **copy.deepcopy(feedback)}]
            elif not isinstance(value, list):
                raise ValueError(f"{name}.{direction} 必须为字段数组、feedback 或 none")
            for field in command[direction]:
                if isinstance(field.get("enum"), str):
                    reference = field["enum"]
                    if reference not in enums:
                        raise ValueError(f"{name}.{direction} 未知枚举 {reference}")
                    field["enum"] = copy.deepcopy(enums[reference])
            _normalize_fields(command[direction], sizes, maximum, f"{name}.{direction}")
        for special, raw in command.get("special_requests", {}).items():
            if not _IDENTIFIER.fullmatch(special):
                raise ValueError(f"{name}.special_requests 名称无效")
            try:
                binary = bytes.fromhex(raw.removeprefix("0x"))
            except (TypeError, ValueError) as error:
                raise ValueError(f"{name}.{special} 不是十六进制字节串") from error
            if not binary or len(binary) > maximum:
                raise ValueError(f"{name}.{special} 长度无效")
        commands.append(command)
    if not commands:
        raise ValueError("commands 不能为空")
    result = copy.deepcopy(cmd_doc)
    result["commands"] = commands
    return result


def _layout(field, offset, size, path):
    for key, expected in (("offset", offset), ("size", size)):
        if key in field and field[key] != expected:
            raise ValueError(f"{path}.{field['name']}.{key} 应为 {expected}")
        field[key] = expected


def _normalize_fields(fields, sizes, maximum, path):
    cursor = 0
    previous_i4 = None
    dynamic_seen = False
    names = set()
    for field in fields:
        name, typ = field.get("name"), field.get("type")
        if not isinstance(name, str) or not _IDENTIFIER.fullmatch(name) or name in names:
            raise ValueError(f"{path} 的字段名称无效或重复：{name!r}")
        names.add(name)
        if "$ref" in field:
            raise ValueError(f"{path}.{name} 不支持 $ref；请直接使用 feedback")
        if typ in _DYNAMIC or field.get("repeat"):
            if dynamic_seen:
                raise ValueError(f"{path} 仅允许一个尾部动态字段")
            _layout(field, cursor, None, path)
            dynamic_seen = True
            continue
        if dynamic_seen or typ not in sizes:
            raise ValueError(f"{path}.{name} 类型无效或动态字段不在结尾")
        size = sizes[typ]
        if typ == "FLD_I4":
            bits = field.get("bits")
            if bits not in ([0, 3], [4, 7]):
                raise ValueError(f"{path}.{name} 的 FLD_I4 必须声明 bits")
            if previous_i4 is not None and bits == [4, 7]:
                offset = previous_i4
            else:
                offset = cursor
                cursor += 1
            previous_i4 = offset
        else:
            offset = cursor
            cursor += size
            previous_i4 = None
        _layout(field, offset, size, path)
    if cursor > maximum:
        raise ValueError(f"{path} 固定部分超过 max_payload")
    by_name = {field["name"]: field for field in fields}
    for field in fields:
        for ref in ("id_ref", "count_ref", "repeat", "scale_exp_ref"):
            if ref in field and field[ref] not in by_name:
                raise ValueError(f"{path}.{field['name']} 引用了不存在的 {ref}")
        if field.get("scale_exp") and field["type"] != "FLD_I4":
            raise ValueError(f"{path}.{field['name']} 的 scale_exp 仅用于 FLD_I4")
        if "scale_exp_ref" in field:
            target = by_name[field["scale_exp_ref"]]
            if field["type"] != "FLD_I16" or not target.get("scale_exp"):
                raise ValueError(f"{path}.{field['name']} 的 scale_exp_ref 无效")
        if "scale" in field and (not isinstance(field["scale"], (int, float)) or field["scale"] <= 0):
            raise ValueError(f"{path}.{field['name']} 的 scale 必须为正数")
        if field.get("overflow", "error") not in ("error", "saturate"):
            raise ValueError(f"{path}.{field['name']} 的 overflow 无效")
        if field["size"] is None and field["type"] == "DYNAMIC":
            if field.get("value_source") not in ("PARAMETERS", "OBSDATA"):
                raise ValueError(f"{path}.{field['name']} 的 value_source 无效")
            if bool(field.get("id_ref")) == bool(field.get("context_ref")):
                raise ValueError(f"{path}.{field['name']} 必须二选一指定 id_ref/context_ref")
        if field.get("repeat"):
            target = by_name[field["repeat"]]
            if target["type"] != "FLD_U8" or target["size"] != 1:
                raise ValueError(f"{path}.{field['name']} 的 repeat 必须引用 U8")
