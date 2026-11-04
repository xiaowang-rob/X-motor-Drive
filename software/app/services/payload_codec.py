"""上位机的 payload 编解码；protocol.payload 只提供 JSON 生成的布局常量。"""

import math
import struct

from protocol import Didx, OBSDATA, PARAMETERS, Pidx, eMotorState
from protocol import codec
from protocol import payload as layout

STATUS_STREAM_IDS = (
    Didx.DATA_MOTOR_STATE,
    Didx.DATA_FAULT,
    Didx.DATA_VBUS,
    Didx.DATA_DRIVER_TEMP,
    Didx.DATA_MOTOR_TEMP,
)
MAX_STREAM_CHANNELS = layout.CMD_DATA_STREAM_SET_REQUEST_IDS_MAX_COUNT


def _finite(value):
    value = float(value)
    if not math.isfinite(value):
        raise ValueError("数值必须有限")
    return value


def _integer(value, low, high, saturate=False):
    value = _finite(value)
    raw = math.floor(value + 0.5) if value >= 0 else math.ceil(value - 0.5)
    if saturate:
        return min(high, max(low, raw))
    if not low <= raw <= high:
        raise ValueError("整数越界")
    return raw


def _pack(buf, fmt, offset, value):
    try:
        struct.pack_into(fmt, buf, offset, value)
    except (struct.error, OverflowError) as error:
        raise ValueError("字段数值越界") from error


def _length(payload, size):
    if len(payload) != size:
        raise ValueError(f"payload 长度错误：需要 {size}，实际 {len(payload)}")


def value_size(meta):
    return getattr(layout, meta.val_type + "_SIZE")


def pack_value(meta, value):
    size = value_size(meta)
    if meta.val_type == "FLD_F32":
        raw = _finite(value)
    elif meta.val_type == "FLD_BOOL":
        if value not in (0, 1):
            raise ValueError("BOOL 只能为 0 或 1")
        raw = int(value)
    else:
        bits = 4 if meta.val_type == "FLD_I4" else size * 8
        signed = meta.val_type.startswith("FLD_I")
        raw = _integer(
            value, -(1 << (bits - 1)) if signed else 0, (1 << (bits - int(signed))) - 1
        )
    if meta.val_type == "FLD_I24":
        return raw.to_bytes(size, "little", signed=True)
    if meta.val_type == "FLD_I4":
        return bytes([raw & 0x0F])
    buf = bytearray(size)
    _pack(buf, getattr(layout, meta.val_type + "_FORMAT"), 0, raw)
    return bytes(buf)


def unpack_value(meta, data, offset=0):
    size = value_size(meta)
    if offset < 0 or offset + size > len(data):
        raise ValueError("字段数据不足")
    if meta.val_type == "FLD_I24":
        value = int.from_bytes(data[offset : offset + size], "little", signed=True)
    elif meta.val_type == "FLD_I4":
        value = data[offset] & 0x0F
        value = value - 16 if value & 8 else value
    else:
        value = struct.unpack_from(
            getattr(layout, meta.val_type + "_FORMAT"), data, offset
        )[0]
        if meta.val_type == "FLD_F32":
            _finite(value)
        elif meta.val_type == "FLD_BOOL":
            if value not in (0, 1):
                raise ValueError("BOOL 字节无效")
            value = bool(value)
    return value, offset + size


def pack_param(meta, value):
    if meta.id not in PARAMETERS:
        raise ValueError("未知参数 ID")
    raw = pack_value(meta, value)
    buf = bytearray(layout.CMD_PARAM_WRITE_REQUEST_FIXED_SIZE + len(raw))
    buf[layout.CMD_PARAM_WRITE_REQUEST_ID_OFFSET] = meta.id
    buf[layout.CMD_PARAM_WRITE_REQUEST_VALUE_OFFSET :] = raw
    return bytes(buf)


def pack_param_read(meta):
    if meta.id not in PARAMETERS:
        raise ValueError("未知参数 ID")
    buf = bytearray(layout.CMD_PARAM_READ_REQUEST_SIZE)
    buf[layout.CMD_PARAM_READ_REQUEST_ID_OFFSET] = meta.id
    return bytes(buf)


def special_request(command, special):
    """参数写入应用 / 全部重置共用 parameter.yaml 里的 PARAM_ALL。"""
    if (command, special) not in (("CMD_PARAM_WRITE", "apply"),
                                  ("CMD_PARAM_RELOAD", "all")):
        raise ValueError("未知特殊请求")
    return bytes([Pidx.PARAM_ALL])


def unpack_param(payload):
    if len(payload) < layout.CMD_PARAM_READ_RESPONSE_FIXED_SIZE:
        raise ValueError("参数回包为空")
    meta = PARAMETERS.get(payload[layout.CMD_PARAM_READ_RESPONSE_ID_OFFSET])
    if meta is None:
        raise ValueError("未知参数 ID")
    value, end = unpack_value(
        meta, payload, layout.CMD_PARAM_READ_RESPONSE_VALUE_OFFSET
    )
    _length(payload, end)
    return meta, value


def pack_params_all_write(records):
    """旧本地批量缓存格式，线上仍按单写命令发送。"""
    if not records:
        raise ValueError("没有可写参数")
    start = records[0][0].id
    raw = bytearray([start, len(records)])
    for index, (meta, value) in enumerate(records):
        if meta.id != start + index:
            raise ValueError("批量参数 ID 必须连续")
        raw.extend(pack_value(meta, value))
    return bytes(raw)


def unpack_params_all_read(payload):
    """旧本地批量缓存格式，不代表线上 CMD_PARAM_READ 回包。"""
    if len(payload) < 2:
        raise ValueError("参数批量回包为空")
    start, count, offset = payload[0], payload[1], 2
    records = []
    for index in range(count):
        meta = PARAMETERS.get(start + index)
        if meta is None:
            raise ValueError("未知批量参数 ID")
        value, offset = unpack_value(meta, payload, offset)
        records.append((meta, value))
    _length(payload, offset)
    return start, records


def _channels(channel_ids):
    ids = tuple(channel_ids)
    if len(ids) > MAX_STREAM_CHANNELS or any(ident not in OBSDATA for ident in ids):
        raise ValueError("示波通道数量或 ID 无效")
    return ids


def pack_stream_set(channel_ids):
    ids = _channels(channel_ids)
    buf = bytearray(layout.CMD_DATA_STREAM_SET_REQUEST_FIXED_SIZE + len(ids))
    buf[layout.CMD_DATA_STREAM_SET_REQUEST_COUNT_OFFSET] = len(ids)
    buf[layout.CMD_DATA_STREAM_SET_REQUEST_IDS_OFFSET :] = bytes(ids)
    return bytes(buf)


def pack_heartbeat(status_values):
    if len(status_values) != len(STATUS_STREAM_IDS):
        raise ValueError("心跳状态块数量不匹配")
    state, fault, vbus, driver_temp, motor_temp = status_values
    return codec.encode(
        "CMD_HEARTBEAT_RESPONSE",
        {"state": state, "fault": fault, "vbus": vbus,
         "driver_temp": driver_temp, "motor_temp": motor_temp},
    )


def unpack_heartbeat(payload):
    values = codec.decode("CMD_HEARTBEAT_RESPONSE", payload)
    return [values[name] for name in
            ("state", "fault", "vbus", "driver_temp", "motor_temp")]


def pack_data_stream(wave_channels=(), wave_values=()):
    channels, values = _channels(wave_channels), tuple(wave_values)
    if len(channels) != len(values):
        raise ValueError("示波通道和值数量不匹配")
    buf = bytearray(layout.CMD_DATA_STREAM_RESPONSE_FIXED_SIZE)
    buf[layout.CMD_DATA_STREAM_RESPONSE_COUNT_OFFSET] = len(values)
    buf.extend(b"".join(pack_value(OBSDATA[c], v) for c, v in zip(channels, values)))
    return bytes(buf)


def unpack_data_stream(payload, channel_ids):
    channels = _channels(channel_ids)
    if len(payload) < layout.CMD_DATA_STREAM_RESPONSE_FIXED_SIZE:
        raise ValueError("示波回包为空")
    count = payload[layout.CMD_DATA_STREAM_RESPONSE_COUNT_OFFSET]
    if count > len(channels):
        raise ValueError("示波通道上下文不足")
    offset, values = layout.CMD_DATA_STREAM_RESPONSE_VALUES_OFFSET, []
    for channel in channels[:count]:
        value, offset = unpack_value(OBSDATA[channel], payload, offset)
        values.append(value)
    _length(payload, offset)
    return values


def pack_connect(state):
    return codec.encode("CMD_CONNECT_REQUEST", {"state": int(bool(state))})


def pack_ctrl_config(mode, control=0):
    """CMD_CTRL_CONFIG 的 2 字节分支：控制模式 + 该模式的子控制字节。"""
    variants = {
        eMotorState.MOTOR_OPENLOOP: "OPENLOOP",
        eMotorState.MOTOR_CURRENT_MODE: "CURRENT",
        eMotorState.MOTOR_PID_SPEED: "PID_SPEED",
        eMotorState.MOTOR_PID_POSITION: "PID_POSITION",
    }
    try:
        suffix = variants[int(mode)]
    except (KeyError, TypeError, ValueError):
        raise ValueError("未知控制模式") from None
    return codec.encode(
        "CMD_CTRL_CONFIG_REQUEST_" + suffix, {"mode": mode, "control": control})


def pack_ctrl_config_mit(
    kp, kd, scale=layout.CMD_CTRL_CONFIG_MIT_REQUEST_SCALE_DEFAULT
):
    """CMD_CTRL_CONFIG 的 MIT 分支：kp/kd 共用同一指数，线上为 3 字节无符号。"""
    return codec.encode(
        "CMD_CTRL_CONFIG_REQUEST_MIT",
        {"mode": eMotorState.MOTOR_MIT_MODE, "scale": scale, "kp": kp, "kd": kd},
    )


def pack_openloop_ctrl(duty, value):
    return codec.encode("CMD_OPENLOOP_CTRL_REQUEST", {"duty": duty, "value": value})


def pack_current_torque(value):
    return codec.encode("CMD_CUR_TAU_CTRL_REQUEST", {"value": value})


def pack_single(value):
    return codec.encode("CMD_PID_VEL_CTRL_REQUEST", {"velocity": value})


def pack_mit_ctrl(
    position,
    velocity=0.0,
    torque=0.0,
    velocity_exp=layout.CMD_MIT_CTRL_REQUEST_VELOCITY_SCALE_DEFAULT,
    torque_exp=layout.CMD_MIT_CTRL_REQUEST_TORQUE_SCALE_DEFAULT,
):
    for exponent in (velocity_exp, torque_exp):
        if _finite(exponent) != int(exponent) or not 0 <= exponent <= 8:
            raise ValueError("MIT 配置指数必须为 0..8 的整数")
    # 指数字段按“正指数”传入：引擎的 SCALE_SIGNED_EXP 负责在线上存负值。
    return codec.encode(
        "CMD_MIT_CTRL_REQUEST",
        {"position": position,
         "velocity_scale": int(velocity_exp),
         "torque_scale": int(torque_exp),
         "velocity": velocity, "torque": torque},
    )


def pack_iap_enter(version, size):
    buf = bytearray(layout.CMD_IAP_ENTER_REQUEST_SIZE)
    _pack(
        buf,
        "<I",
        layout.CMD_IAP_ENTER_REQUEST_VERSION_OFFSET,
        _integer(version, 0, 0xFFFFFF),
    )
    _pack(
        buf,
        "<I",
        layout.CMD_IAP_ENTER_REQUEST_SIZE_OFFSET,
        _integer(size, 8, 0xFFFFFFFF),
    )
    return bytes(buf)


def pack_iap_erase(size):
    buf = bytearray(layout.CMD_IAP_ERASE_FLASH_REQUEST_SIZE)
    _pack(
        buf,
        "<I",
        layout.CMD_IAP_ERASE_FLASH_REQUEST_SIZE_OFFSET,
        _integer(size, 0, 0xFFFFFFFF),
    )
    return bytes(buf)


def pack_iap_write(offset, data):
    data = bytes(data)
    if len(data) > layout.CMD_IAP_WRITE_FLASH_REQUEST_DATA_MAX_SIZE:
        raise ValueError("IAP 数据过长")
    buf = bytearray(layout.CMD_IAP_WRITE_FLASH_REQUEST_FIXED_SIZE + len(data))
    _pack(
        buf,
        "<I",
        layout.CMD_IAP_WRITE_FLASH_REQUEST_OFFSET_OFFSET,
        _integer(offset, 0, 0xFFFFFFFF),
    )
    buf[layout.CMD_IAP_WRITE_FLASH_REQUEST_DATA_OFFSET :] = data
    return bytes(buf)


def unpack_iap_write_ack(payload):
    _length(payload, layout.CMD_IAP_WRITE_FLASH_RESPONSE_SIZE)
    return (
        struct.unpack_from(
            "<I", payload, layout.CMD_IAP_WRITE_FLASH_RESPONSE_OFFSET_OFFSET
        )[0],
        payload[layout.CMD_IAP_WRITE_FLASH_RESPONSE_STATUS_OFFSET],
    )


def pack_iap_verify(crc32, size):
    buf = bytearray(layout.CMD_IAP_VERIFY_FLASH_REQUEST_SIZE)
    _pack(
        buf,
        "<I",
        layout.CMD_IAP_VERIFY_FLASH_REQUEST_CRC32_OFFSET,
        _integer(crc32, 0, 0xFFFFFFFF),
    )
    _pack(
        buf,
        "<I",
        layout.CMD_IAP_VERIFY_FLASH_REQUEST_SIZE_OFFSET,
        _integer(size, 0, 0xFFFFFFFF),
    )
    return bytes(buf)
