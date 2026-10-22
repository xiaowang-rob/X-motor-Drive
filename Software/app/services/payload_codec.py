"""二进制 payload 编解码（小端）。

约定：
- 参数：类型感知 TLV，单条 [id:u8][raw N]；批量 [start:u8][count:u8][raw...]
- 心跳：固定状态块 + 最多 5 路示波订阅值
- 控制：定长小端结构
- IAP：offset/size/crc32 为 u32
"""

import struct

from protocol import Didx, OBSDATA, PARAMETERS


# 心跳固定状态块，顺序必须与固件 _stream_send() 一致。
STATUS_STREAM_IDS = (
    Didx.DATA_STATE,
    Didx.DATA_FAULT,
    Didx.DATA_WARNING,
    Didx.DATA_VBUS,
    Didx.DATA_DRIVER_TEMP,
    Didx.DATA_MOTOR_TEMP,
)
MAX_STREAM_CHANNELS = 5


VAL_TYPE_FORMAT = {
    "FLD_U8": "<B",
    "FLD_U16": "<H",
    "FLD_U32": "<I",
    "FLD_F32": "<f",
    "FLD_BOOL": "<B",
}
VAL_TYPE_SIZE = {
    "FLD_U8": 1,
    "FLD_U16": 2,
    "FLD_U32": 4,
    "FLD_F32": 4,
    "FLD_BOOL": 1,
}


def value_size(meta):
    return VAL_TYPE_SIZE[meta.val_type]


def pack_value(meta, value):
    fmt = VAL_TYPE_FORMAT[meta.val_type]
    if meta.val_type == "FLD_BOOL":
        value = 1 if value else 0
    return struct.pack(fmt, value)


def unpack_value(meta, data, offset=0):
    fmt = VAL_TYPE_FORMAT[meta.val_type]
    size = VAL_TYPE_SIZE[meta.val_type]
    if offset + size > len(data):
        raise ValueError(f"payload 长度不足：{meta.id_name}")
    value = struct.unpack_from(fmt, data, offset)[0]
    if meta.val_type == "FLD_BOOL":
        value = bool(value)
    return value, offset + size


def pack_param(meta, value):
    return bytes([meta.id]) + pack_value(meta, value)


def unpack_param(payload):
    if len(payload) < 1:
        raise ValueError("参数读回包为空")
    meta = PARAMETERS.get(payload[0])
    if meta is None:
        raise ValueError(f"未知参数 ID：{payload[0]}")
    value, _ = unpack_value(meta, payload, 1)
    return meta, value


def pack_params_all_write(records):
    """records: [(meta, value), ...]，要求 meta.id 连续。"""
    if not records:
        raise ValueError("没有可写参数")
    start = records[0][0].id
    for offset, (meta, _) in enumerate(records):
        if meta.id != start + offset:
            raise ValueError("批量参数 ID 必须连续")
    payload = bytearray([start, len(records)])
    for meta, value in records:
        payload.extend(pack_value(meta, value))
    return bytes(payload)


def unpack_params_all_read(payload):
    """返回 (start_id, [(meta, value), ...])；count=0 表示结束帧。"""
    if len(payload) < 2:
        raise ValueError("参数批量回包为空")
    start = payload[0]
    count = payload[1]
    offset = 2
    records = []
    for index in range(count):
        meta = PARAMETERS.get(start + index)
        if meta is None:
            raise ValueError(f"未知批量参数 ID：{start + index}")
        value, offset = unpack_value(meta, payload, offset)
        records.append((meta, value))
    return start, records


def pack_stream_set(channel_ids):
    if len(channel_ids) > MAX_STREAM_CHANNELS:
        raise ValueError(f"示波数据流最多 {MAX_STREAM_CHANNELS} 路")
    return bytes([len(channel_ids)]) + bytes(channel_ids)


def pack_heartbeat(status_values, wave_channels=(), wave_values=()):
    """心跳回包：count + 固定状态块 + 订阅的示波值。"""
    if len(status_values) != len(STATUS_STREAM_IDS):
        raise ValueError("心跳状态块数量不匹配")
    if len(wave_values) > MAX_STREAM_CHANNELS:
        raise ValueError(f"示波数据流最多 {MAX_STREAM_CHANNELS} 路")
    payload = bytearray([len(wave_values)])
    for channel_id, value in zip(STATUS_STREAM_IDS, status_values):
        payload.extend(pack_value(OBSDATA[channel_id], value))
    for channel_id, value in zip(wave_channels, wave_values):
        payload.extend(pack_value(OBSDATA[channel_id], value))
    return bytes(payload)


def unpack_heartbeat(payload, channel_ids):
    """返回 (固定状态值列表, 示波值列表)。"""
    if not payload:
        raise ValueError("心跳回包为空")
    count = payload[0]
    if count > MAX_STREAM_CHANNELS:
        raise ValueError(f"示波数据流超过 {MAX_STREAM_CHANNELS} 路")
    offset = 1
    status_values = []
    for channel_id in STATUS_STREAM_IDS:
        value, offset = unpack_value(OBSDATA[channel_id], payload, offset)
        status_values.append(value)
    wave_values = []
    for index in range(min(count, len(channel_ids))):
        meta = OBSDATA.get(channel_ids[index])
        if meta is None:
            raise ValueError(f"未知示波通道 ID：{channel_ids[index]}")
        value, offset = unpack_value(meta, payload, offset)
        wave_values.append(value)
    return status_values, wave_values


def pack_connect(state):
    return bytes([1 if state else 0])


def pack_openloop(duty, value):
    return struct.pack("<ff", float(duty), float(value))


def pack_current_torque(is_torque, value):
    return struct.pack("<Bf", 1 if is_torque else 0, float(value))


def pack_single(value):
    return struct.pack("<f", float(value))


def pack_mit_full(kp, kd, pos, vel, tau_ff):
    return struct.pack("<fffff", float(kp), float(kd), float(pos),
                       float(vel), float(tau_ff))


def pack_mit_config(kp, kd):
    return struct.pack("<ff", float(kp), float(kd))


def pack_mit_vt(vel, tau_ff):
    return struct.pack("<ff", float(vel), float(tau_ff))


def pack_iap_erase(size):
    return struct.pack("<I", int(size))


def pack_iap_write(offset, data):
    if len(data) > 124:
        raise ValueError("IAP 单块数据超过 124 字节")
    return struct.pack("<I", int(offset)) + bytes(data)


def unpack_iap_write_ack(payload):
    if len(payload) < 5:
        raise ValueError("IAP 写入应答长度不足")
    offset, status = struct.unpack_from("<IB", payload, 0)
    return offset, status


def pack_iap_verify(crc32, size):
    return struct.pack("<II", int(crc32), int(size))
