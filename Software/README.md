# XDr 上位机

`start.py` 启动新 UI，并由 `app/services/device_service.py` 接管串口、参数、配置、
日志、波形和固件页面动作。payload 使用小端二进制；参数、数据流、控制、IAP 的
编解码分别在上位机 `app/services/payload_codec.py` 和固件 `cmd_process.c` 中实现。

## 协议生成

在仓库根执行：

```bash
python3 protocol/gen_protocol.py
```

输入为 `protocol/fieldvaluetype.json`、`parameter.json`、`obsdata.json`、`cmd.json`，输出：

- `firmware/protocol/fieldvaluetype.h`
- `firmware/protocol/parameter_list.h`
- `firmware/protocol/data_list.h`
- `firmware/protocol/cmd_list.h`
- `Software/protocol/__init__.py`

参数/观测 ID 由脚本按 JSON 字段顺序自动分配（从 `0x01` 开始），JSON 中不写 `id`。
Python 协议包提供 `Pidx`、`Didx`、`CmdId`、`FbId`、`UartFrame`、选项枚举，
以及 `PARAMETERS` / `OBSDATA` 字段元数据和 `COMMANDS` / `FRAME` / `CMD_TARGETS` 元数据。

## 运行

需要 `PyQt5`、`PyQt-Fluent-Widgets`、`pyserial`、`pyqtgraph`、`numpy`：

```bash
cd Software
python3.13 start.py
```

## 业务层

- `app/services/serial_service.py`：端口扫描、连接/断开、心跳、协议组帧/拆帧。
- `app/services/device_service.py`：把页面信号映射到命令 ID；参数/配置/波形/IAP 动作入口。
- `app/services/config_store.py`：本地参数预设读写。
- `app/waveform_widget.py`：基于 `pyqtgraph` 的五通道实时波形控件。

串口帧格式与固件一致：`[head][id][len][payload][crc8][tail]`，帧头/帧尾来自
`protocol/__init__.py` 的 `UartFrame`。

## Payload 协议

所有多字节字段为小端。

- 连接：`CMD_CONNECT` payload `[state:u8]`，1=连接，0=断连。
- 心跳/数据流：上位机每 200ms 发 `CMD_HEARTBEAT`；下位机回
  `CMD_HEARTBEAT` payload `[count:u8][固定状态块][订阅值...]`。
  固定状态块顺序：`state:u8 / fault:u8 / warning:u8 / vbus:f32 /
  driver_temp:f32 / motor_temp:f32`。上位机 1s 无回包判定断连。
- 数据流订阅：`CMD_DATA_STREAM_SET` payload `[count:u8][id...]`，
  只用于示波，最多 5 路，与示波 5 个通道一致；回包按订阅顺序、按
  `obsdata` 类型编码。
- 参数单读：`CMD_PARAM_READ` `[id:u8]` → `[id:u8][raw N]`。
- 参数单写：`CMD_PARAM_WRITE` `[id:u8][raw N]` → `[status:u8]`。
- 参数全读：`CMD_PARAM_ALLREAD` 空 → 多帧 `[start:u8][count:u8][raw...]`，
  `count=0` 结束。
- 参数全写：`CMD_PARAM_ALLWRITE` `[start:u8][count:u8][raw...]` → `[status:u8]`。
- 开环：`[duty:f32][value:f32]`；角度命令 `CMD_OPENLOOP_THETA`，速度命令
  `CMD_OPENLOOP_VEL`。
- 电流/力矩：`CMD_CUR_TAU_MODE` `[is_torque:u8][value:f32]`。
- PID：`CMD_PID_VEL` / `CMD_PID_POS` `[value:f32]`。
- MIT：USB/UART 用 `CMD_MIT_POS` 单帧 20B `[kp][kd][pos][vel][tau_ff]`；
  CAN 用 `CMD_MIT_POS`(pos)、`CMD_MIT_CONFIG`(kp,kd)、`CMD_MIT_VT`(vel,tau_ff)
  三帧。固件按 `CMD_MIT_POS` 帧长 4/20B 兼容。
- 设置限位：`CMD_SET_LIMIT_POS` 空 payload，以当前位置为正/负极限位置
  （当前位置 > 0 写最大位置，否则写最小位置）。
- IAP：擦除 `[size:u32]`；写入 `[offset:u32][data≤120B]` →
  `[offset:u32][status:u8]`；校验 `[crc32:u32][size:u32]`；退出后设备复位。

校准命令本身无 payload；上位机点击校准时先按参数 TLV 写入校准参数，再发送
对应 `CMD_*_TUNNING` 命令。

由于当前只允许修改 `cmd_process.c`，`comm_process()` 需要主循环周期调用；
如果尚未接线，心跳/数据流不会自动回包，校准服务也会因缺少全局 `tTune` 返回失败。

## 配置

`Software/config/` 同时保存主题配置和参数预设：

- `config.json`：QFluentWidgets 主题配置，不参与预设列表。
- `default.json`：默认参数预设。
- 其他 `*.json`：用户保存的参数预设。

## 页面

- **配置**：电机 / 观测器 / 控制器 / 轨迹规划器 / 通讯 / 保护 六个 tab。
  - 参数列表使用 `ScrollCard`，参数多时只滚动卡片内部。
  - 校准使用 `CalibrationCard` 固定在页面底部，不随参数滚动。
  - 通讯 tab 位于保护之前，包含 CAN 配置和 RS485 预留卡片。
- **监听**：CAN / 485 两个空页，后续接入监听解析。
- **控制**：波形通道选项来自协议 `obsdata`；位置 / 速度 / 电流每行末尾都有独立的 SEND 图标按钮。
  - 空闲模式不建导航项；模式下拉可跳转导航页，但切换导航页不回写下拉框。
  - 开环的角度/速度选择器、电流/力矩选择器直接作为滑条标题。
  - MIT 的 CSV、周期和“开始执行”按钮在同一行。
  - 控制区固定在底部，左侧按钮与右侧导航之间有分割线，示波区保留剩余高度。
- **固件**：设备信息只读且不带操作按钮；驱动器信息为终端式输出框；选择固件文件使用输入框后的图标按钮。
- **日志**：只读下位机日志输出区。

参数默认值以 `protocol/parameter.json` 为准，`Software/config/default.json` 由协议默认值生成；
保存配置、删除配置和校准说明等弹窗使用 QFluentWidgets 控件。

## 扩展点

- 参数分组、页面顺序、校准参数/命令/help 文本：`Software/app/config_page.py` 顶部常量。
- 示波可选信号与通道数：`Software/app/control_page.py` 的 `WAVE_CHANNELS` / `WAVE_CHANNEL_COUNT`。
- 参数/观测字段的名称、单位、默认值、选项：直接改 `protocol/*.json` 后重新生成协议。
- 协议 payload：上位机在 `Software/app/services/payload_codec.py`，固件在
  `firmware/app/app/cmd_process.c`；IAP 分块上传在 `Software/app/services/iap_worker.py`。

## 测试

```bash
cd Software
QT_QPA_PLATFORM=offscreen python3.13 -m unittest test_ui -v
```

`test_ui.py` 验证导航、配置页结构、滚动卡片、校准卡片、控制模式、监听页、
控制页、固件页和几何布局。
