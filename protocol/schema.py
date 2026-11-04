"""协议 YAML 的加载、规范化和校验。"""
import copy
import re
from pathlib import Path

import yaml

_IDENTIFIER = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
_DYNAMIC = {"DYNAMIC", "BYTES", "STRING"}


def load_yaml(path):
    """读取协议 YAML；返回结构与原 JSON 完全一致的 Python 对象。"""
    return yaml.safe_load(Path(path).read_text(encoding="utf-8"))


def _observed_enums(obs):
    """obsdata 字段的 options 就是协议枚举；数值为成员在表中的顺序。"""
    enums = {}
    for field in (obs or {}).get("fields", []):
        options = field.get("options")
        if not options:
            continue
        name = options["enum_name"]
        values = {item["id_name"]: index
                  for index, item in enumerate(options["items"])}
        if name in enums and enums[name] != values:
            raise ValueError(f"obsdata 枚举 {name} 定义不一致")
        enums[name] = values
    return enums


def _resolve_enum_members(members, observed, path):
    """把 enum_from 的成员名解析为 {成员名: 值}，成员必须来自同一个 obsdata 枚举。"""
    if not isinstance(members, list) or not members:
        raise ValueError(f"{path} 的 enum_from 必须是非空成员名数组")
    matches = [values for values in observed.values()
               if all(member in values for member in members)]
    if len(matches) != 1:
        raise ValueError(f"{path} 的 enum_from 成员无法唯一匹配 obsdata 枚举")
    return {member: matches[0][member] for member in members}


def _resolve_field_enum(field, enums, observed, path):
    """enum 可为本地枚举名；enum_from 直接引用 obsdata 枚举成员。"""
    reference = field.pop("enum_from", None)
    if reference is None:
        reference = field.get("enum")
        if isinstance(reference, str):
            if reference not in enums:
                raise ValueError(f"{path} 未知枚举 {reference}")
            field["enum"] = copy.deepcopy(enums[reference])
        return
    field["enum"] = _resolve_enum_members(reference, observed, f"{path}.{field['name']}")
    if field["type"] != "FLD_U8":
        raise ValueError(f"{path}.{field['name']} 的 enum_from 仅用于 FLD_U8")


def normalize(cmd_doc, value_types, obs=None):
    """返回不含快捷写法、具备 offset/size 的不可变协议模型。

    obs 为 obsdata.yaml；提供后字段可用 enum_from 引用其枚举，避免重复写值。
    """
    if cmd_doc.get("schema_version") != 1:
        raise ValueError("cmd.yaml schema_version 必须为 1")
    if cmd_doc.get("endian") != "little":
        raise ValueError("当前只支持 little endian")
    maximum = cmd_doc.get("max_payload")
    if not isinstance(maximum, int) or not 1 <= maximum <= 255:
        raise ValueError("max_payload 必须是 1..255 的整数")

    sizes = {name: spec["size"] for name, spec in value_types.items()}
    observed = _observed_enums(obs)
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
                _resolve_field_enum(field, enums, observed, f"{name}.{direction}")
            _normalize_fields(command[direction], sizes, maximum, f"{name}.{direction}")
        variants = command.get("request_variants", {})
        if variants:
            selector = command.get("request_selector")
            if selector not in {f["name"] for f in command["request"]}:
                raise ValueError(f"{name} 的 request_selector 无效")
            selected = set()
            for variant, fields in variants.items():
                if not _IDENTIFIER.fullmatch(variant):
                    raise ValueError(f"{name} 的分支名无效")
                for field in fields:
                    _resolve_field_enum(field, enums, observed, f"{name}.{variant}")
                _normalize_fields(fields, sizes, maximum, f"{name}.{variant}")
                if not fields or fields[0]["name"] != selector:
                    raise ValueError(f"{name}.{variant} 缺少首字节选择字段")
                values = list(fields[0].get("enum", {}).values())
                if len(values) != 1 or values[0] in selected:
                    raise ValueError(f"{name}.{variant} 的模式必须唯一")
                selected.add(values[0])
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
        for ref in ("id_ref", "count_ref", "repeat", "scale_exp_ref", "scale_ref"):
            if ref in field and field[ref] not in by_name:
                raise ValueError(f"{path}.{field['name']} 引用了不存在的 {ref}")
        if "scale_ref" in field:
            target = by_name[field["scale_ref"]]
            if (field["type"] != "FLD_U24" or target["type"] != "FLD_U8"
                    or target["offset"] >= field["offset"]
                    or "scale" in field or "scale_exp_ref" in field):
                raise ValueError(f"{path}.{field['name']} 的 scale_ref 必须引用前置 U8 指数字段")
        if field.get("scale_exp") and field["type"] != "FLD_I4":
            raise ValueError(f"{path}.{field['name']} 的 scale_exp 仅用于 FLD_I4")
        if "scale_exp_ref" in field:
            target = by_name[field["scale_exp_ref"]]
            if field["type"] != "FLD_I16" or not target.get("scale_exp"):
                raise ValueError(f"{path}.{field['name']} 的 scale_exp_ref 无效")
        if "scale" in field and (type(field["scale"]) is not int or not -32 <= field["scale"] <= 32):
            raise ValueError(f"{path}.{field['name']} 的 scale 必须为 -32..32 的整数指数")
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
