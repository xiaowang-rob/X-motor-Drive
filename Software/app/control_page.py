"""同屏显示波形与手动控制，不连接设备。"""
from PyQt5.QtCore import Qt, QRectF
from PyQt5.QtGui import QPainter, QPainterPath, QColor, QPen
from PyQt5.QtWidgets import (
    QFileDialog, QHBoxLayout, QLabel, QSizePolicy, QStackedWidget,
    QVBoxLayout, QWidget,
)
from qfluentwidgets import (
    BodyLabel, CheckBox, ComboBox, DoubleSpinBox, FluentIcon as FIF,
    HorizontalSeparator, LineEdit, PushButton, Slider, VerticalSeparator,
    isDarkTheme, qconfig, setCustomStyleSheet,
)
from qfluentwidgets.components.navigation import SegmentedWidget

from protocol import Didx, OBSDATA, eCoreState
from app.ui_components import ContentPage, SectionCard, icon_action, preview_button
from app.waveform_widget import WaveformWidget


CHANNEL_COLORS = ("#58cfc2", "#f5ae50", "#e77fa3", "#9cce70", "#b59af7")

# 示波可选信号：直接改这个元组即可；下拉名称来自 protocol 的 OBSDATA。
WAVE_CHANNELS = (
    Didx.DATA_CURRENT_U,
    Didx.DATA_CURRENT_V,
    Didx.DATA_CURRENT_W,
    Didx.DATA_VOLTAGE_Q,
    Didx.DATA_VOLTAGE_D,
    Didx.DATA_CURRENT_Q,
    Didx.DATA_CURRENT_D,
    Didx.DATA_CURRENT_Q_REF,
    Didx.DATA_VELOCITY,
    Didx.DATA_VELOCITY_REF,
    Didx.DATA_THETA_ELEC,
    Didx.DATA_THETA_MECH,
    Didx.DATA_POSITION,
    Didx.DATA_POSITION_REF,
)

# 通道行数只由 WAVE_CHANNEL_COUNT 决定；WAVE_CHANNELS 只决定每行下拉的可选项。
WAVE_CHANNEL_COUNT = 5

# 控制模式来自 obsdata 的 eCoreState；顺序即左侧下拉和右侧顶部导航的顺序。
MODE_PAGE_MODES =  (
    ("开环模式", eCoreState.CORE_OPENLOOP),
    ("电流/力矩", eCoreState.CORE_CURRENT_MODE),
    ("PID速度", eCoreState.CORE_PID_SPEED),
    ("PID位置", eCoreState.CORE_PID_POSITION),
    ("MIT", eCoreState.CORE_MIT_MODE),
)
MODE_STACK_HEIGHT = 96


class WaveCanvas(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.channel_colors = CHANNEL_COLORS
        self.setMinimumSize(360, 240)
        self.setAccessibleName("波形显示")
        qconfig.themeChangedFinished.connect(self.update)

    def paintEvent(self, event):
        painter = QPainter(self)
        dark = isDarkTheme()
        painter.setRenderHint(QPainter.Antialiasing)
        border = QRectF(self.rect()).adjusted(0.5, 0.5, -0.5, -0.5)
        path = QPainterPath()
        path.addRoundedRect(border, 8, 8)
        painter.fillPath(path, QColor("#191d24" if dark else "#f5f7fa"))
        painter.setClipPath(path)
        plot = self.rect().adjusted(48, 20, -20, -36)
        painter.setPen(QPen(QColor("#303844" if dark else "#e1e6ed"), 1))
        for i in range(11):
            x = plot.left() + plot.width() * i / 10
            painter.drawLine(int(x), plot.top(), int(x), plot.bottom())
        for i in range(7):
            y = plot.top() + plot.height() * i / 6
            painter.drawLine(plot.left(), int(y), plot.right(), int(y))
        painter.setPen(QColor("#aeb6c2" if dark else "#59636f"))
        painter.drawText(12, 18, "幅值")
        painter.drawText(self.rect().adjusted(0, 0, -20, -8), Qt.AlignBottom | Qt.AlignRight, "时间 / s")
        painter.setClipping(False)
        painter.setPen(QPen(QColor("#353d49" if dark else "#dce2ea"), 1))
        painter.drawRoundedRect(border, 8, 8)
        painter.end()


class TargetControl(QWidget):
    """按起点到终点映射滑条，支持终点小于起点及零跨度。"""
    def __init__(self, name, unit, endpoint, parent=None, *, label_width=100):
        super().__init__(parent)
        self.name = name
        self.unit = unit
        row = QHBoxLayout(self)
        row.setContentsMargins(0, 0, 0, 0)
        row.setSpacing(8)
        self.label = BodyLabel(self._title(name, unit))
        self.label.setFixedWidth(label_width)
        row.addWidget(self.label)
        self.minimum = QLabel("0")
        self.minimum.setFixedWidth(28)
        self.minimum.setAlignment(Qt.AlignCenter)
        self.minimum.setAccessibleName(name + "最小值")
        self.minimum.setToolTip("最小值")
        self.maximum = self._line_edit(name + "最大值", str(endpoint), 96)
        self.maximum.setToolTip("最大值")
        self.target = self._line_edit(name + "目标值", "0", 96)
        self.target.setToolTip("目标值 / " + unit)
        setCustomStyleSheet(
            self.target,
            "LineEdit { background-color: #edf4ff; border: 1px solid #9ebce8; }",
            "LineEdit { background-color: #223349; border: 1px solid #42658e; }",
        )
        self.slider = Slider(Qt.Horizontal)
        self.slider.setRange(0, 10000)
        self.slider.setAccessibleName(name + "目标滑条")
        row.addWidget(self.minimum)
        row.addWidget(self.slider, 1)
        row.addWidget(self.maximum)
        row.addWidget(self.target)
        self.send_button = icon_action(FIF.SEND, f"发送{name}目标")
        self.send_button.setProperty("deviceAction", True)
        self.send_button.setEnabled(False)
        row.addWidget(self.send_button)
        self.slider.valueChanged.connect(self._from_slider)
        self.target.editingFinished.connect(self._from_target)
        self.maximum.editingFinished.connect(self._update_range)
        self._update_range()

    @staticmethod
    def _title(name, unit):
        return f"{name}({unit})" if unit else name

    def set_spec(self, name, unit, endpoint):
        """切换同一滑条代表的控制量（开环角度/速度、电流/力矩）。"""
        self.name = name
        self.unit = unit
        self.label.setText(self._title(name, unit))
        self.minimum.setAccessibleName(name + "最小值")
        self.maximum.setAccessibleName(name + "最大值")
        self.maximum.setToolTip("最大值")
        self.target.setAccessibleName(name + "目标值")
        self.target.setToolTip("目标值 / " + unit)
        self.slider.setAccessibleName(name + "目标滑条")
        self.send_button.setAccessibleName(f"发送{name}目标")
        self.send_button.setToolTip(f"发送{name}目标")
        self.maximum.setText(f"{endpoint:g}")
        self.target.setText("0.00")
        self._update_range()

    @staticmethod
    def _line_edit(name, value, width=96):
        edit = LineEdit()
        edit.setText(value)
        edit.setFixedSize(width, 32)
        edit.setAccessibleName(name)
        return edit

    @staticmethod
    def _number(edit, fallback=0.0):
        try:
            return float(edit.text().strip())
        except (TypeError, ValueError):
            edit.setText(f"{fallback:g}")
            return fallback

    def _update_range(self):
        end = self._number(self.maximum, 0.0)
        self.maximum.setText(f"{end:g}")
        self.slider.setEnabled(end != 0.0)
        if end == 0.0:
            self.target.setText("0.00")
            self.slider.setValue(0)
        else:
            self._from_target()

    def _from_slider(self, position):
        end = self._number(self.maximum, 0.0)
        value = end * position / 10000
        self.target.blockSignals(True)
        self.target.setText(f"{value:.2f}")
        self.target.blockSignals(False)

    def _from_target(self):
        end = self._number(self.maximum, 0.0)
        value = self._number(self.target, 0.0)
        if end != 0.0:
            value = max(min(value, max(0.0, end)), min(0.0, end))
        self.target.setText(f"{value:.2f}")
        position = round(value / end * 10000) if end != 0.0 else 0
        self.slider.blockSignals(True)
        self.slider.setValue(position)
        self.slider.blockSignals(False)

    def value(self):
        return self._number(self.target, 0.0)


class ControlPage(ContentPage):
    def __init__(self):
        super().__init__("control_page")
        content_widget = QWidget()
        content = QVBoxLayout(content_widget)
        content.setContentsMargins(0, 0, 0, 4)
        content.setSpacing(12)
        self.main_layout.addWidget(content_widget, 1)
        self.control_content = content_widget
        row = QHBoxLayout()
        row.setSpacing(12)
        self.channel_card = SectionCard("采样通道")
        self.channel_card.setFixedWidth(192)
        self.wave_ch, self.channel_badges = [], []
        wave_colors = tuple(
            CHANNEL_COLORS[index % len(CHANNEL_COLORS)]
            for index in range(WAVE_CHANNEL_COUNT)
        )
        for index in range(1, WAVE_CHANNEL_COUNT + 1):
            color = wave_colors[index - 1]
            channel = QVBoxLayout()
            channel.setContentsMargins(0, 0, 0, 0)
            channel.setSpacing(4)
            combo = ComboBox()
            combo.setMinimumWidth(0)
            combo.setFixedHeight(32)
            combo.addItem("未选择", userData=None)
            for candidate in WAVE_CHANNELS:
                meta = OBSDATA[candidate]
                combo.addItem(meta.name, userData=candidate)
            combo.setAccessibleName(f"通道 {index} 信号")
            line = QLabel()
            line.setFixedHeight(3)
            line.setAccessibleName(f"通道 {index} 颜色")
            line.setStyleSheet(f"background-color: {color}; border-radius: 1px;")
            channel.addWidget(combo)
            channel.addWidget(line)
            self.channel_card.content.addLayout(channel)
            self.wave_ch.append(combo)
            self.channel_badges.append(line)
        self.channel_card.content.addWidget(HorizontalSeparator())
        self.auto_x_check = CheckBox("自动 X 轴")
        self.auto_x_check.setChecked(True)
        self.channel_card.content.addWidget(self.auto_x_check)
        self.auto_y_check = CheckBox("自动 Y 轴")
        self.auto_y_check.setChecked(True)
        self.channel_card.content.addWidget(self.auto_y_check)
        self.wave_start_button = preview_button("开始采样")
        self.channel_card.content.addWidget(self.wave_start_button)
        self.wave_clear_button = PushButton("清空")
        self.wave_clear_button.setFixedHeight(32)
        self.channel_card.content.addWidget(self.wave_clear_button)
        row.addWidget(self.channel_card)
        plot = SectionCard("波形")
        self.wave_canvas = WaveformWidget()
        self.wave_canvas.channel_colors = wave_colors
        plot.content.addWidget(self.wave_canvas, 1)
        row.addWidget(plot, 1)
        content.addLayout(row, 1)
        self.wave_start_button.clicked.connect(self._toggle_wave)
        self.wave_clear_button.clicked.connect(self._clear_wave)
        self.auto_x_check.toggled.connect(self.wave_canvas.set_auto_x_scale)
        self.auto_y_check.toggled.connect(self.wave_canvas.set_auto_y_scale)
        self._wave_running = False

        self.control_card = SectionCard("手动控制")
        self.control_card.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        self.control_card.content.setContentsMargins(12, 10, 12, 12)
        control_body = QHBoxLayout()
        control_body.setSpacing(12)

        self.control_side = QWidget()
        self.control_side.setFixedWidth(120)
        side = QVBoxLayout(self.control_side)
        side.setContentsMargins(0, 0, 0, 0)
        side.setSpacing(8)
        self.action_buttons = []
        for text in ["释放电机", "设置零点", "设置限位"]:
            button = preview_button(text)
            button.setFixedWidth(120)
            side.addWidget(button)
            self.action_buttons.append(button)
        side.addStretch()
        control_body.addWidget(self.control_side)

        divider = VerticalSeparator()
        divider.setFixedWidth(1)
        divider.setMinimumHeight(140)
        control_body.addWidget(divider)

        self.enable_buttons = []
        self.mode_nav = SegmentedWidget()
        self.mode_stack = QStackedWidget()
        self.mode_stack.setFixedHeight(MODE_STACK_HEIGHT)
        self.mode_routes = []
        self._mode_syncing = False
        builders = (
            self._build_openloop_page,
            self._build_current_torque_page,
            self._build_pid_speed_page,
            self._build_pid_position_page,
            self._build_mit_page,
        )
        for index, ((label, _), builder) in enumerate(
                zip(MODE_PAGE_MODES, builders)):
            route = f"control_mode_{index}"
            self.mode_routes.append(route)
            self.mode_nav.addItem(route, label)
            self.mode_stack.addWidget(builder())

        right = QVBoxLayout()
        right.setContentsMargins(0, 0, 0, 0)
        right.setSpacing(8)
        right.addWidget(self.mode_nav)
        right.addWidget(self.mode_stack, 1)
        control_body.addLayout(right, 1)
        self.control_card.content.addLayout(control_body)
        content.addWidget(self.control_card)

        # 保留旧 API 的三个目标控件：位置 / 速度 / 电流
        self.target_controls = [
            self.position_control,
            self.speed_control,
            self.current_control,
        ]
        self.sliders = [control.slider for control in self.target_controls]
        self.targets = [control.target for control in self.target_controls]

        self.mode_nav.currentItemChanged.connect(self._on_mode_route_changed)
        self._apply_mode(0)

        self.bind_parameters()
        for button, action in zip(self.action_buttons, ("disable", "set_zero", "set_limit")):
            self.bind_action(button, action)

    def _make_target(self, name, unit, endpoint, label_width=100):
        control = TargetControl(name, unit, endpoint, label_width=label_width)
        control.send_button.clicked.connect(
            lambda _=False, c=control: self._send_target(c))
        return control

    def _mode_page(self, selector, *controls):
        """模式页：左侧使能/选择列，右侧滑条或 CSV 控制。"""
        page = QWidget()
        row = QHBoxLayout(page)
        row.setContentsMargins(0, 0, 0, 0)
        row.setSpacing(10)

        left = QVBoxLayout()
        left.setContentsMargins(0, 0, 0, 0)
        left.setSpacing(8)
        enable = preview_button("使能", primary=True)
        enable.setFixedWidth(100)
        enable.setAccessibleName("使能电机")
        enable.clicked.connect(lambda: self.action_requested.emit("enable", None))
        left.addWidget(enable)
        self.enable_buttons.append(enable)
        if selector is not None:
            selector.setFixedWidth(100)
            selector.setFixedHeight(32)
            left.addWidget(selector)
        left.addStretch()
        row.addLayout(left)

        divider = VerticalSeparator()
        divider.setFixedWidth(1)
        divider.setMinimumHeight(60)
        row.addWidget(divider)

        right = QVBoxLayout()
        right.setContentsMargins(0, 0, 0, 0)
        right.setSpacing(8)
        for control in controls:
            right.addWidget(control)
        right.addStretch()
        row.addLayout(right, 1)
        return page

    @staticmethod
    def _page(*items):
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(8)
        for item in items:
            if isinstance(item, QHBoxLayout):
                layout.addLayout(item)
            else:
                layout.addWidget(item)
        layout.addStretch()
        return page

    def _build_openloop_page(self):
        self.openloop_kind = ComboBox()
        self.openloop_kind.setFixedHeight(32)
        self.openloop_kind.addItem("开环角度", userData=0)
        self.openloop_kind.addItem("开环速度", userData=1)
        self.openloop_kind.setAccessibleName("开环输入")
        self.openloop_duty = self._make_target("占空比", "%", 100)
        self.openloop_control = self._make_target("开环角度", "rad", 6.28)
        self.openloop_kind.currentIndexChanged.connect(
            self._on_openloop_kind_changed)
        return self._mode_page(
            self.openloop_kind, self.openloop_duty, self.openloop_control)

    def _on_openloop_kind_changed(self, index):
        if index == 0:
            self.openloop_control.set_spec("开环角度", "rad", 6.28)
        else:
            self.openloop_control.set_spec("开环速度", "rad/s", 200)

    def _build_current_torque_page(self):
        self.current_torque_kind = ComboBox()
        self.current_torque_kind.setFixedHeight(32)
        self.current_torque_kind.addItem("电流", userData=0)
        self.current_torque_kind.addItem("力矩", userData=1)
        self.current_torque_kind.setAccessibleName("控制量")
        self.current_torque_control = self._make_target("电流", "A", 10)
        self.current_control = self.current_torque_control
        self.current_torque_kind.currentIndexChanged.connect(
            self._on_current_torque_kind_changed)
        return self._mode_page(
            self.current_torque_kind, self.current_torque_control)

    def _on_current_torque_kind_changed(self, index):
        if index == 0:
            self.current_torque_control.set_spec("电流", "A", 10)
        else:
            self.current_torque_control.set_spec("力矩", "N·m", 5)

    def _build_pid_speed_page(self):
        self.speed_control = self._make_target("速度", "rad/s", 100)
        return self._mode_page(None, self.speed_control)

    def _build_pid_position_page(self):
        self.position_control = self._make_target("位置", "rad", 180)
        return self._mode_page(None, self.position_control)

    def _build_mit_page(self):
        self.mit_position = self._make_target("位置", "rad", 180)

        execution = QHBoxLayout()
        execution.setContentsMargins(0, 0, 0, 0)
        execution.setSpacing(8)
        execution.addWidget(BodyLabel("CSV"))
        self.mit_csv_path = LineEdit()
        self.mit_csv_path.setReadOnly(True)
        self.mit_csv_path.setFixedHeight(32)
        self.mit_csv_path.setPlaceholderText("选择 CSV 轨迹文件")
        self.mit_csv_button = icon_action(FIF.FOLDER, "选择 CSV 文件")
        self.mit_csv_button.clicked.connect(self._choose_mit_csv)
        execution.addWidget(self.mit_csv_path, 1)
        execution.addWidget(self.mit_csv_button)

        execution.addWidget(BodyLabel("周期(ms)"))
        self.mit_period = DoubleSpinBox()
        self.mit_period.setRange(1, 60000)
        self.mit_period.setDecimals(0)
        self.mit_period.setValue(100)
        self.mit_period.setFixedSize(120, 32)
        execution.addWidget(self.mit_period)

        self.mit_config_send = preview_button("开始执行", primary=True)
        self.mit_config_send.setIcon(FIF.PLAY)
        self.mit_config_send.setFixedHeight(32)
        self.mit_config_send.clicked.connect(self._send_mit_config)
        execution.addWidget(self.mit_config_send)

        self.mit_kind = ComboBox()
        self.mit_kind.setFixedHeight(32)
        self.mit_kind.addItem("滑条控制", userData=0)
        self.mit_kind.addItem("CSV控制", userData=1)
        self.mit_kind.setAccessibleName("MIT 控制方式")

        self.mit_stack = QStackedWidget()
        self.mit_stack.setFixedHeight(72)
        self.mit_stack.addWidget(self._page(self.mit_position))
        self.mit_stack.addWidget(self._page(execution))
        self.mit_kind.currentIndexChanged.connect(self.mit_stack.setCurrentIndex)

        return self._mode_page(self.mit_kind, self.mit_stack)

    def _choose_mit_csv(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "选择 MIT CSV 轨迹", "", "CSV 文件 (*.csv)")
        if path:
            self.mit_csv_path.setText(path)

    def _send_mit_config(self):
        self.action_requested.emit(
            "send_mit_config",
            {
                "csv": self.mit_csv_path.text(),
                "period_ms": self.mit_period.value(),
            },
        )

    def _on_mode_route_changed(self, route):
        if route in self.mode_routes:
            self._apply_mode(self.mode_routes.index(route))

    def _apply_mode(self, index):
        if self._mode_syncing or not 0 <= index < len(MODE_PAGE_MODES):
            return
        self._mode_syncing = True
        self.current_mode = MODE_PAGE_MODES[index][1]
        self.mode_stack.setCurrentIndex(index)
        self.mode_nav.setCurrentItem(self.mode_routes[index])
        self._mode_syncing = False

    def set_mode(self, core_state):
        for index, (_, state) in enumerate(MODE_PAGE_MODES):
            if state == core_state:
                self._apply_mode(index)
                return
        if core_state == eCoreState.CORE_IDLE:
            self.current_mode = eCoreState.CORE_IDLE
            return
        raise ValueError(f"未知控制模式：{core_state}")

    def get_mode(self):
        return self.current_mode

    def _toggle_wave(self):
        self._wave_running = not self._wave_running
        if self._wave_running:
            self.wave_canvas.start()
            self.wave_start_button.setText("停止采样")
            channels = [
                (index, combo.currentData())
                for index, combo in enumerate(self.wave_ch)
                if combo.currentData() is not None
            ]
            self.action_requested.emit("wave_start", {"channels": channels})
        else:
            self.wave_canvas.pause()
            self.wave_start_button.setText("开始采样")
            self.action_requested.emit("wave_stop", None)

    def _clear_wave(self):
        self.wave_canvas.clear_waveforms()

    def add_wave_data(self, channel, value):
        self.wave_canvas.add_waveform_data(int(channel), float(value))

    def _send_target(self, control):
        channel = control.target.accessibleName().replace("目标值", "")
        self.action_requested.emit(
            "send_target", {"channel": channel, "value": control.value()})

    def get_targets(self):
        return {control.objectName() or control.target.accessibleName(): control.target.text()
                for control in self.target_controls}

    def set_targets(self, values):
        for control in self.target_controls:
            key = control.target.accessibleName().replace("目标值", "")
            if key in values:
                control.target.setText(str(values[key]))
                control.target.editingFinished.emit()

    def set_wave_channels(self, channels):
        for combo, value in zip(self.wave_ch, channels):
            index = combo.findText(value) if isinstance(value, str) else combo.findData(value)
            if index >= 0:
                combo.setCurrentIndex(index)
