"""上位机命令 payload 的手写编解码；字段偏移、格式和缩放在此维护。"""

import math
import struct

from protocol import CmdId, Didx, OBSDATA, PARAMETERS, Pidx, eMotorState

STATUS_STREAM_IDS = (
    Didx.DATA_MOTOR_STATE,
    Didx.DATA_FAULT,
    Didx.DATA_VBUS,
    Didx.DATA_DRIVER_TEMP,
    Didx.DATA_MOTOR_TEMP,
)
MAX_STREAM_CHANNELS = 5
IAP_MAX_DATA = 124

# 参数和观测项仍使用生成的元数据，基础值的字节表示由上位机维护。
_VALUE_FORMATS = {
    "FLD_U8": "<B", "FLD_U16": "<H", "FLD_U32": "<I",
    "FLD_F32": "<f", "FLD_BOOL": "<B", "FLD_I16": "<h",
}
_VALUE_SIZES = {
    "FLD_U8": 1, "FLD_U16": 2, "FLD_U32": 4, "FLD_F32": 4,
    "FLD_BOOL": 1, "FLD_I4": 1, "FLD_I16": 2, "FLD_I24": 3, "FLD_U24": 3,
}


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
    return _VALUE_SIZES[meta.val_type]


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
    if meta.val_type in ("FLD_I24", "FLD_U24"):
        return raw.to_bytes(size, "little", signed=meta.val_type == "FLD_I24")
    if meta.val_type == "FLD_I4":
        return bytes([raw & 0x0F])
    buf = bytearray(size)
    _pack(buf, _VALUE_FORMATS[meta.val_type], 0, raw)
    return bytes(buf)


def unpack_value(meta, data, offset=0):
    size = value_size(meta)
    if offset < 0 or offset + size > len(data):
        raise ValueError("字段数据不足")
    if meta.val_type in ("FLD_I24", "FLD_U24"):
        value = int.from_bytes(data[offset : offset + size], "little", signed=meta.val_type == "FLD_I24")
    elif meta.val_type == "FLD_I4":
        value = data[offset] & 0x0F
        value = value - 16 if value & 8 else value
    else:
        value = struct.unpack_from(
            _VALUE_FORMATS[meta.val_type], data, offset
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
    return bytes([meta.id]) + raw


def pack_param_read(meta):
    if meta.id not in PARAMETERS:
        raise ValueError("未知参数 ID")
    return bytes([meta.id])


def special_request(command, special):
    """参数写入应用 / 全部重置共用 parameter.yaml 里的 PARAM_ALL。"""
    if (command, special) not in (("CMD_PARAM_WRITE", "apply"),
                                  ("CMD_PARAM_RELOAD", "all")):
        raise ValueError("未知特殊请求")
    return bytes([Pidx.PARAM_ALL])


def unpack_param(payload):
    if len(payload) < 1:
        raise ValueError("参数回包为空")
    meta = PARAMETERS.get(payload[0])
    if meta is None:
        raise ValueError("未知参数 ID")
    value, end = unpack_value(
        meta, payload, 1
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
    return bytes([len(ids), *ids])


def pack_heartbeat(status_values):
    """[state:u8][fault:u8][vbus:u16 / 2^8][driver_temp:u8][motor_temp:u8]。"""
    if len(status_values) != len(STATUS_STREAM_IDS):
        raise ValueError("心跳状态块数量不匹配")
    state, fault, vbus, driver_temp, motor_temp = status_values
    buf = bytearray(6)
    _pack(buf, "<B", 0, _integer(state, 0, 255))
    _pack(buf, "<B", 1, _integer(fault, 0, 255))
    _pack(buf, "<H", 2, _integer(_finite(vbus) * 256, 0, 65535))
    _pack(buf, "<B", 4, _integer(driver_temp, 0, 255))
    _pack(buf, "<B", 5, _integer(motor_temp, 0, 255))
    return bytes(buf)


def unpack_heartbeat(payload):
    _length(payload, 6)
    state, fault, vbus, driver_temp, motor_temp = struct.unpack("<BBHBB", payload)
    return [state, fault, vbus / 256.0, driver_temp, motor_temp]


def pack_data_stream(wave_channels=(), wave_values=()):
    channels, values = _channels(wave_channels), tuple(wave_values)
    if len(channels) != len(values):
        raise ValueError("示波通道和值数量不匹配")
    buf = bytearray([len(values)])
    buf.extend(b"".join(pack_value(OBSDATA[c], v) for c, v in zip(channels, values)))
    return bytes(buf)


def unpack_data_stream(payload, channel_ids):
    channels = _channels(channel_ids)
    if len(payload) < 1:
        raise ValueError("示波回包为空")
    count = payload[0]
    if count > len(channels):
        raise ValueError("示波通道上下文不足")
    offset, values = 1, []
    for channel in channels[:count]:
        value, offset = unpack_value(OBSDATA[channel], payload, offset)
        values.append(value)
    _length(payload, offset)
    return values


def pack_calibration(command, values):
    """校准输入直接放入命令 payload，不经过参数读写协议。"""
    fields = {
        CmdId.CMD_RL_TUNNING: ("tune_current",),
        CmdId.CMD_PSIF_TUNNING: ("tune_vel",),
        CmdId.CMD_MECH_TUNNING: ("tune_current", "tune_vel"),
        CmdId.CMD_OBS_TUNNING: ("tune_current", "tune_vel"),
        CmdId.CMD_PID_TUNNING: ("tune_current", "tune_vel"),
        CmdId.CMD_MIT_TUNNING: ("tune_current", "tune_vel"),
    }.get(command)
    if fields is None:
        raise ValueError("未知校准命令")
    buf = bytearray(4 * len(fields))
    for index, field in enumerate(fields):
        _pack(buf, "<f", index * 4, _finite(values[field]))
    return bytes(buf)


def pack_connect(state):
    return bytes([int(bool(state))])


def pack_ctrl_config(mode, control=0):
    """普通分支：[mode:u8][control:u8]。"""
    if mode not in (eMotorState.MOTOR_OPENLOOP, eMotorState.MOTOR_CURRENT_MODE,
                    eMotorState.MOTOR_PID_SPEED, eMotorState.MOTOR_PID_POSITION):
        raise ValueError("未知控制模式")
    return bytes([_integer(mode, 0, 255), _integer(control, 0, 255)])


def pack_ctrl_config_mit(kp, kd, scale=16):
    """[mode:u8][scale:u8][kp:u24][kd:u24]；kp/kd 共用 2^scale 缩放。"""
    scale = _integer(scale, 0, 255)
    factor = 1 << scale
    kp_raw = _integer(_finite(kp) * factor, 0, 0xFFFFFF)
    kd_raw = _integer(_finite(kd) * factor, 0, 0xFFFFFF)
    return (bytes([eMotorState.MOTOR_MIT_MODE, scale])
            + kp_raw.to_bytes(3, "little") + kd_raw.to_bytes(3, "little"))


def pack_openloop_ctrl(duty, value):
    """[duty:u16 / 2^8][value:f32]，占空比按协议范围饱和。"""
    buf = bytearray(6)
    _pack(buf, "<H", 0, _integer(_finite(duty) * 256, 0, 65535, saturate=True))
    _pack(buf, "<f", 2, _finite(value))
    return bytes(buf)


def pack_current_torque(value):
    return pack_single(value)


def pack_single(value):
    buf = bytearray(4)
    _pack(buf, "<f", 0, _finite(value))
    return bytes(buf)


def pack_mit_ctrl(position, velocity=0.0, torque=0.0, velocity_exp=6, torque_exp=8):
    """[position:i24 / 2^18][负指数低/高半字节][velocity:i16][torque:i16]。"""
    for exponent in (velocity_exp, torque_exp):
        if _finite(exponent) != int(exponent) or not 0 <= exponent <= 8:
            raise ValueError("MIT 配置指数必须为 0..8 的整数")
    position_raw = _integer(_finite(position) * (1 << 18), -0x800000, 0x7FFFFF,
                            saturate=True)
    velocity_raw = _integer(_finite(velocity) * (1 << int(velocity_exp)),
                            -32768, 32767, saturate=True)
    torque_raw = _integer(_finite(torque) * (1 << int(torque_exp)),
                          -32768, 32767, saturate=True)
    scales = (-int(velocity_exp) & 0x0F) | ((-int(torque_exp) & 0x0F) << 4)
    return (position_raw.to_bytes(3, "little", signed=True) + bytes([scales])
            + struct.pack("<hh", velocity_raw, torque_raw))


def pack_iap_enter(version, size):
    buf = bytearray(8)
    _pack(buf, "<I", 0, _integer(version, 0, 0xFFFFFF))
    _pack(buf, "<I", 4, _integer(size, 8, 0xFFFFFFFF))
    return bytes(buf)


def pack_iap_erase(size):
    buf = bytearray(4)
    _pack(buf, "<I", 0, _integer(size, 0, 0xFFFFFFFF))
    return bytes(buf)


def pack_iap_write(offset, data):
    data = bytes(data)
    if len(data) > IAP_MAX_DATA:
        raise ValueError("IAP 数据过长")
    buf = bytearray(4 + len(data))
    _pack(buf, "<I", 0, _integer(offset, 0, 0xFFFFFFFF))
    buf[4:] = data
    return bytes(buf)


def unpack_iap_write_ack(payload):
    _length(payload, 5)
    return struct.unpack("<IB", payload)


def pack_iap_verify(crc32, size):
    buf = bytearray(8)
    _pack(buf, "<I", 0, _integer(crc32, 0, 0xFFFFFFFF))
    _pack(buf, "<I", 4, _integer(size, 0, 0xFFFFFFFF))
    return bytes(buf)
