# XDr 上位机

`start.py` 启动 UI；`app/services/device_service.py` 负责串口、参数、波形和 IAP。所有 payload 为小端二进制，**协议唯一来源是仓库根目录的 `protocol/*.yaml`**。

## 协议生成

```bash
# 在仓库根目录运行：校验 YAML，同时生成 Python 模块和 C 头文件
python3 protocol/gen_protocol.py

# CI / 提交前：确认生成代码没有落后于 YAML
python3 protocol/gen_protocol.py --check

# 可选：覆盖 payload.h 输出位置（其他 C 头仍生成到 firmware/protocol/）
python3 protocol/gen_protocol.py --c-output /tmp/payload.h
```

输入文件：

- `protocol/cmd.yaml`：帧常量、命令 ID、请求/回复字段、通用反馈和特殊请求；payload 已内嵌在各命令中。
- `protocol/fieldvaluetype.yaml`：字段的线宽与 C/Python 类型映射。
- `protocol/parameter.yaml`、`protocol/obsdata.yaml`：参数、观测项、单位、默认值和选项。

输出：

- `Software/protocol/__init__.py`：命令/字段常量、参数和观测元数据。
- `Software/protocol/payload.py`：由 JSON 生成的通用 `encode()` / `decode()` 及字段编解码。
- `firmware/protocol/`：`fieldvaluetype.h`、`cmd_list.h`、`parameter_list.h`、`data_list.h` 和 `payload.h`。

`app/services/payload_codec.py` 是兼容 UI/IAP 调用的薄封装，不能在其中增加字段偏移或 `struct.pack` 格式。参数与观测 ID 按 JSON 数组顺序从 `0x01` 自动分配；因此已发布协议中不可插入、删除或重排字段，只能追加，否则后续 ID 会变化。

### JSON 规则

- 固定字段按数组顺序紧密排列，`offset` / `size` 由生成器推导，不手填。
- `scale` 统一为编码 `raw = round(value * scale)`、解码 `value = raw / scale`，采用半远离零舍入。
- `overflow` 默认报错；显式 `"saturate"` 才饱和裁剪。
- `FLD_I4` 共享字节必须声明 `bits: [0, 3]` 或 `[4, 7]`。
- `DYNAMIC`、`BYTES`、`STRING` 是请求/回复尾部动态字段；动态观测数据和数据流需要 ID/订阅上下文。
- `special_requests` 定义完整特殊字节请求，例如参数应用的 `FF`；上位机从生成 schema 读取，不能直接写字面量。

## USB 设备识别

安装 USB 枚举依赖（在仓库根目录执行）：

```bash
python3 -m pip install -r software/requirements.txt
```

运行环境还需提供 libusb 1.0 后端。Linux 通常由发行版的 `libusb-1.0` 软件包提供，并需通过 udev 规则授予当前用户设备访问权限；Windows 需安装可被 PyUSB 加载的 libusb 后端，并为 APP、BL 对应的 USB 设备分别绑定 WinUSB（可用 Zadig）。

设备列表每 2.5 秒扫描一次，显示 `(USB)XDr-S`、`(USB)XDr-S BL`、`(USB)XDr-P` 或 `(USB)XDr-P BL`。内部仍按 VID/PID 和总线地址定位选中的设备。ID 引用由 `protocol/devices.yaml` 生成的 `protocol/devices.py`，不再维护旧 PID 表。缺少 PyUSB 或 USB 后端时会在日志中报告扫描失败，串口和蓝牙扫描仍可继续。

选择 USB 后点击连接，将认领 Vendor 接口 0 / alt 0，通过 OUT `0x01`、IN `0x81` 进行 Bulk 收发；不转换成串口、不嵌套 UART 帧。打开成功后沿用现有 `CMD_CONNECT`、心跳、参数和数据流处理。与原串口流程一致，界面“已连接”表示传输已打开，不代表设备业务已确认；一秒没有有效回包将报告心跳超时。APP 必须实现 USB 命令处理和回包后才能正常通信，本次上位机接入不替代固件业务实现。

USB 帧：`A5 5A | id:u8 | len:u16 LE | data | CRC16 LE`。`len` 只计算数据，范围 0..4095；CRC16/CCITT-FALSE（poly `0x1021`、init `0xFFFF`）覆盖头部和数据。支持半帧、连续多帧、CRC/非法长度重新同步和半帧空闲超时；64 字节整倍数的发送追加 ZLP。收发异常及手动断开会停止线程、释放 USB 接口并清空待发送队列。

尚未进行实机 USB 收发、热插拔及 IAP 验证。IAP 仍保留现有整区 CRC 校验 TODO，此次只接通底层传输，不代表完整升级流程已验证。

## 传输与业务

串口帧：`[head][id][len][payload][crc8(id,len,payload)][tail]`。最大 payload 为 128 字节。

- 参数单读：`CMD_PARAM_READ [id] -> [id][raw N]`；单写：`CMD_PARAM_WRITE [id][raw N]`。
- 示波订阅：`CMD_DATA_STREAM_SET [count][id...]`，最多 5 路；`CMD_DATA_STREAM` 的值类型和顺序由订阅上下文决定。
- MIT：位置为 `i24/2^18`；速度、力矩的配置指数为正数 6/8，线上写入负指数。
- IAP：写入 `[offset:u32][data<=124B] -> [offset:u32][status:u8]`。

生成脚本直接同步 Python 模块与 C 协议头文件，不修改固件业务 handler。业务 handler 仍需调用 `payload.h` 中有长度检查的 `proto_encode_*` / `proto_decode_*` 接口，生成文件更新本身不等于完成业务接入。

## 测试

协议测试不依赖 Qt：

```bash
PYTHONPATH=Software python3 -B -m unittest Software.test_protocol -v
python3 protocol/gen_protocol.py --check
python3 protocol/gen_protocol.py --c-output /tmp/payload.h
printf '#include "/tmp/payload.h"\nint main(void) { return 0; }\n' | cc -x c -std=c11 -Wall -Wextra -Werror -o /tmp/payload-check -
```

UI 回归测试仍为：

```bash
cd Software
QT_QPA_PLATFORM=offscreen python3.13 -m unittest test_ui -v
```

它需要 Python、PyQt5、QFluentWidgets 和 `cffi` 后端版本一致。
