# XDr 上位机 UI

`start.py` 进入纯 UI 界面，不初始化串口、采样、参数读写或固件烧录。
原 `functions/`、`protocol.py`、`data_page.py` 与 `data_ui_map.py` 保留，暂未接入。

## 运行

需要 `PyQt5` 和 `PyQt-Fluent-Widgets`：

```bash
cd Software
python3 start.py
```

- 全局顶部：连接、上位机执行输出、复位、主题及设备状态。
- 配置：左侧五类参数顶部导航，右侧配置文件选择与图标操作栏；悬停图标显示操作名称。
- 电机参数分为内参整定（含电气参数）、机械参数整定；控制参数包含 CAN ID 和 CAN 模式。
- 控制：波形、采样通道、滑条和控制按钮同页显示。
- 固件：单页展示设备型号、引导程序及应用固件版本，以及两类固件的烧录入口。
- 日志：单页只读下位机日志输出区。
- 所有参数行统一使用 `ParameterInput`：标题后显示单位（如 `相电阻（Ω）`），输入框固定宽度，并提供读取、写入、恢复默认按钮；下拉参数使用 `ComboBox`。
- 页面通过 `parameter_read_requested`、`parameter_write_requested`、`parameter_default_requested`、`action_requested` 信号对外发布请求，通过 `set_parameter`、`set_parameters` 等方法回填数据，不要求业务层直接操作控件。
- 状态栏使用 `update_status` 回填母线电压、温度、运行状态和错误等参数值；复位按钮通过 `action_requested` 发布操作。
- 主题与本地输入可交互；设备按钮默认禁用，业务层可调用 `set_device_enabled`。
- 上下位机输出框默认不填充模拟记录，可通过日志页 API 更新。
- 最小窗口尺寸为 1180 × 800，长内容垂直滚动。

## 测试

```bash
cd Software
QT_QPA_PLATFORM=offscreen python3 -m unittest test_ui -v
```

`test_ui.py` 验证导航层级、配置结构、单页合并、输出区、设备逻辑隔离、主题及布局。
`test_all.py` 为旧业务界面的历史测试，不适用于当前纯 UI 入口。
