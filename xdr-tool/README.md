# XDr 上位机

`start.py` 启动 UI；`app/services/device_service.py` 负责串口、参数、波形和 IAP。所有 payload 为小端二进制，由上下位机各自手工维护编解码。

## 协议生成与维护

```bash
# 在仓库根目录运行：生成命令枚举、参数/观测元数据和设备常量
python3 protocol/gen_protocol.py

# 确认生成文件与 YAML 元数据同步
python3 protocol/gen_protocol.py --check
```

输入文件：

- `protocol/cmd.yaml`：生成命令 ID、帧常量和反馈枚举；其中请求/回复的 payload 描述仅供查阅，不参与代码生成或布局校验。
- `protocol/fieldvaluetype.yaml`：生成固件的字段类型枚举和字节数查询。
- `protocol/parameter.yaml`、`protocol/obsdata.yaml`：参数、观测项、单位、默认值和选项。
- `protocol/devices.yaml`：USB VID/PID 等设备常量。

生成输出为 `software/protocol/__init__.py`、`software/protocol/devices.py`，以及 `firmware/protocol/` 下的 `cmd_list.h`、`fieldvaluetype.h`、`parameter_list.h`、`data_list.h`、`devices.h`。`cmd_list.h` 只含 `eCmdId`、`eUartFrame`、`eFeedBack` 等枚举。

payload 在以下位置手工维护，生成脚本不会覆盖：

- `firmware/app/payload.h`：现有 CMD 固定 payload 的装包/解包宏，直接维护字段偏移、缩放和长度；调用方负责接收长度校验。动态 payload 保留在业务 handler 中处理。
- `software/app/services/payload_codec.py`：上位机编码和解码，直接维护字节格式、字段偏移、长度检查和数值范围。

修改 payload 时，按需同步两端实现。YAML 中已有的 payload 描述保留为参考，无须为了编解码修改 YAML 或运行生成脚本。

参数与观测 ID 按 YAML 字段顺序从 `0x01` 自动分配；已发布协议增项时应追加，避免重排引起 ID 变化。参数/观测项的值类型继续引用生成元数据，字节编码由手写代码负责。

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

## 测试

协议测试不依赖 Qt：

```bash
PYTHONPATH=software python3 -B -m unittest software.test_protocol -v
python3 -B -m unittest discover -s protocol -p 'test_*.py' -v
python3 protocol/gen_protocol.py --check
```

UI 回归测试仍为：

```bash
cd software
QT_QPA_PLATFORM=offscreen python3.13 -m unittest test_ui -v
```

它需要 Python、PyQt5、QFluentWidgets 和 `cffi` 后端版本一致。
