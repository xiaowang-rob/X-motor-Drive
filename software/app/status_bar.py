"""常驻全局状态栏；连接操作与设备状态分行呈现。"""
from PyQt5.QtWidgets import QWidget, QHBoxLayout, QVBoxLayout, QSizePolicy
from PyQt5.QtCore import Qt, pyqtSignal
from qfluentwidgets import (
    ComboBox, CaptionLabel, StrongBodyLabel, ToolButton, LineEdit,
    HorizontalSeparator, VerticalSeparator, FluentIcon as FIF,
    Theme, setTheme, isDarkTheme,
)
from app.ui_components import preview_button, SurfaceCard, style_console_line


class StatusBar(QWidget):
    status_changed = pyqtSignal(object, object)
    action_requested = pyqtSignal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        main = QVBoxLayout(self)
        main.setContentsMargins(0, 0, 0, 0)
        self.card = SurfaceCard()
        content = QVBoxLayout(self.card)
        content.setContentsMargins(16, 12, 16, 12)
        content.setSpacing(10)

        controls = QHBoxLayout()
        controls.setSpacing(8)
        self.com_port = ComboBox()
        self.com_port.addItem("未选择端口")
        self.com_port.setFixedSize(180, 32)
        self.com_port.setToolTip("选择连接端口")
        self.connect_but = ToolButton(FIF.LINK)
        self.connect_but.setFixedSize(32, 32)
        self.connect_but.setToolTip("连接设备")
        self.connect_but.setAccessibleName("连接设备")
        self.connect_but.setProperty("deviceAction", True)
        self.connect_but.setEnabled(False)
        controls.addWidget(self.com_port)
        controls.addWidget(self.connect_but)
        controls.addWidget(CaptionLabel("上位机"))
        self.host_output = LineEdit()
        self.host_output.setReadOnly(True)
        self.host_output.setFixedHeight(32)
        self.host_output.setMinimumWidth(140)
        self.host_output.setPlaceholderText("上位机执行输出")
        self.host_output.setFocusPolicy(Qt.NoFocus)
        style_console_line(self.host_output)
        self.host_output.setAccessibleName("上位机执行输出")
        self.host_output.setToolTip("上位机执行输出")
        controls.addWidget(self.host_output, 1)


        separator = VerticalSeparator()
        separator.setFixedHeight(20)
        controls.addSpacing(4)
        controls.addWidget(separator)
        controls.addSpacing(4)
        self.theme_toggle = ToolButton(FIF.BRIGHTNESS)
        self.theme_toggle.setFixedSize(32, 32)
        self.theme_toggle.clicked.connect(self._toggle_theme)
        self._update_theme_hint()
        controls.addWidget(self.theme_toggle)
        content.addLayout(controls)
        content.addWidget(HorizontalSeparator())

        status = QHBoxLayout()
        status.setSpacing(16)
        self.indicators = {}
        items = [
            ("voltage", "母线电压", "—"),
            ("temperature", "设备温度", "—"),
            ("motor_temperature", "电机温度", "—"),
            ("state", "运行状态", "—"),
            ("error", "错误", "—"),
        ]
        for index, (key, title, value) in enumerate(items):
            if index:
                divider = VerticalSeparator()
                divider.setFixedHeight(16)
                status.addWidget(divider)
            group = QHBoxLayout()
            group.setSpacing(10)
            group.addWidget(CaptionLabel(title))
            label = StrongBodyLabel(value)
            label.setAccessibleName(title)
            group.addWidget(label)
            group.addStretch()
            self.indicators[key] = label
            status.addLayout(group, 1)
        self.system_reset_button = preview_button("系统复位")
        self.reset_button = preview_button("核心复位")
        status.addWidget(self.system_reset_button)
        status.addWidget(self.reset_button)
        self.system_reset_button.clicked.connect(lambda: self.action_requested.emit("system_reset"))
        self.reset_button.clicked.connect(lambda: self.action_requested.emit("status_reset"))
        content.addLayout(status)
        main.addWidget(self.card)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)

    def update_status(self, values):
        """由设备数据层回填状态值，values 的 key 对应 voltage 等固定字段。"""
        for key, value in values.items():
            if key in self.indicators:
                self.indicators[key].setText(str(value))
                self.status_changed.emit(key, value)

    def set_host_text(self, text):
        """上位机终端只保留当前一行输出。"""
        self.host_output.setText(str(text))
        self.host_output.setCursorPosition(0)

    def set_device_enabled(self, enabled):
        self.connect_but.setEnabled(enabled)
        for button in (self.system_reset_button, self.reset_button):
            button.setEnabled(enabled)

    def _update_theme_hint(self):
        hint = "切换为浅色主题" if isDarkTheme() else "切换为深色主题"
        self.theme_toggle.setToolTip(hint)
        self.theme_toggle.setAccessibleName(hint)

    def _toggle_theme(self):
        setTheme(Theme.LIGHT if isDarkTheme() else Theme.DARK, save=False)
        self._update_theme_hint()
