"""纯 UI 公共组件，不依赖设备协议或业务模块。"""
from PyQt5.QtCore import Qt, QRectF, pyqtSignal
from app.parameter_input import ParameterInput
from PyQt5.QtGui import QPainter, QFontDatabase
from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGridLayout, QStackedWidget,
    QStyle, QStyleOptionButton,
)
from qfluentwidgets import (
    BodyLabel, CaptionLabel, StrongBodyLabel, ComboBox, LineEdit,
    PushButton, PrimaryPushButton, ToolButton, ToolTipFilter, Pivot, ScrollArea,
    SimpleCardWidget, FluentIcon as FIF, setCustomStyleSheet,
)


class SurfaceCard(SimpleCardWidget):
    """内容容器不响应鼠标悬停变色，仅交互控件突出反馈。"""
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setBorderRadius(8)


class CompactComboBox(ComboBox):
    """窄配置选择器用图标呈现，选项名称保留在下拉菜单与提示中。"""
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedSize(36, 36)
        self.installEventFilter(ToolTipFilter(self))

    def paintEvent(self, event):
        option = QStyleOptionButton()
        self.initStyleOption(option)
        option.text = ""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        self.style().drawControl(QStyle.CE_PushButton, option, painter, self)
        FIF.DOCUMENT.render(painter, QRectF(7, 10, 14, 16))
        FIF.ARROW_DOWN.render(painter, QRectF(25, 16, 6, 6))


def style_console(editor, compact=False):
    """上下位机输出采用同一套随主题变化的终端样式。"""
    font = QFontDatabase.systemFont(QFontDatabase.FixedFont)
    font.setPointSize(10)
    editor.setFont(font)
    editor.setTabChangesFocus(True)
    editor.document().setDocumentMargin(4 if compact else 12)
    for_theme = []
    for background, foreground, border in [
        ("#f5f7fa", "#263244", "#dce2ea"),
        ("#191d24", "#d4dce8", "#353d49"),
    ]:
        for_theme.append(
            f"QPlainTextEdit {{ background-color: {background}; color: {foreground}; "
            f"border: 1px solid {border}; border-radius: 8px; "
            "selection-background-color: #346aa6; selection-color: white; }"
        )
    setCustomStyleSheet(editor, *for_theme)


def preview_button(text, primary=False):
    button = PrimaryPushButton(text) if primary else PushButton(text)
    button.setProperty("deviceAction", True)
    button.setFixedHeight(32)
    button.setAccessibleName(text)
    button.setEnabled(False)
    button.setToolTip(text)
    button.installEventFilter(ToolTipFilter(button))
    return button


def icon_action(icon, text):
    """保留 hover 提示；未接入的操作没有业务信号连接。"""
    button = ToolButton(icon)
    button.setFixedSize(36, 36)
    button.setAccessibleName(text)
    button.setToolTip(text)
    button.installEventFilter(ToolTipFilter(button))
    button.setProperty("previewAction", True)
    return button


def button_row(*texts):
    row = QHBoxLayout()
    row.setSpacing(8)
    for i, text in enumerate(texts):
        row.addWidget(preview_button(text, primary=i == 0))
    row.addStretch()
    return row


class SectionCard(SurfaceCard):
    def __init__(self, title, parent=None):
        super().__init__(parent)
        self.content = QVBoxLayout(self)
        self.content.setContentsMargins(16, 14, 16, 16)
        self.content.setSpacing(12)
        self.fields = {}
        self.parameters = {}
        self.parameter_scope = title

    def add_fields(self, fields, read_only=False, data_type=None):
        for name, spec in fields:
            options = spec if isinstance(spec, (tuple, list)) else None
            config = dict(spec) if isinstance(spec, dict) else {}
            config.setdefault("param_id", f"{self.parameter_scope}/{name}")
            config.setdefault("data_type", data_type or (
                "uint8" if options is not None or name == "极对数" else
                "uint32" if name in ("CAN ID", "分辨率") else "float32"))
            config.setdefault("unit", spec if isinstance(spec, str) else "")
            config.setdefault("options", options)
            config.setdefault("read_only", read_only)
            parameter = ParameterInput(name, **config)
            self.content.addWidget(parameter)
            self.parameters[name] = parameter
            # 保留旧的 editor 索引，业务集成使用 ContentPage 的参数 API。
            self.fields[name] = parameter.editor
        return self


class ContentPage(QWidget):
    """页面统一接口：UI 发布请求，业务层通过方法回填，不直接访问 editor。"""
    parameter_read_requested = pyqtSignal(object)
    parameter_write_requested = pyqtSignal(object, object)
    parameter_default_requested = pyqtSignal(object)
    parameter_changed = pyqtSignal(object, object)
    validation_failed = pyqtSignal(object, str)
    action_requested = pyqtSignal(str, object)

    def bind_parameters(self):
        self.parameters = {}
        for parameter in self.findChildren(ParameterInput):
            if parameter.param_id in self.parameters:
                raise ValueError(f"参数 ID 重复：{parameter.param_id}")
            self.parameters[parameter.param_id] = parameter
            parameter.read_requested.connect(self.parameter_read_requested)
            parameter.write_requested.connect(self.parameter_write_requested)
            parameter.default_requested.connect(self.parameter_default_requested)
            parameter.value_changed.connect(self.parameter_changed)
            parameter.validation_failed.connect(self.validation_failed)

    def get_parameter(self, param_id):
        return self.parameters[param_id].value()

    def set_parameter(self, param_id, value):
        self.parameters[param_id].set_value(value)

    def set_parameters(self, values):
        for param_id, value in values.items():
            self.set_parameter(param_id, value)

    def get_parameters(self):
        """返回非空、可写参数；非法值抛出 ValueError，避免静默写入错误数据。"""
        return {key: parameter.value() for key, parameter in self.parameters.items()
                if not parameter.read_only and
                (isinstance(parameter.editor, ComboBox) or parameter.editor.text().strip())}

    def bind_action(self, button, action, payload=None):
        button.clicked.connect(lambda: self.action_requested.emit(
            action, payload() if callable(payload) else payload))

    def set_device_enabled(self, enabled):
        for parameter in self.parameters.values():
            parameter.set_device_enabled(enabled)
        for button in self.findChildren(QWidget):
            if button.property("deviceAction"):
                button.setEnabled(enabled)

    def __init__(self, name, parent=None):
        super().__init__(parent)
        self.setObjectName(name)
        self.main_layout = QVBoxLayout(self)
        self.main_layout.setContentsMargins(0, 0, 0, 16)
        self.main_layout.setSpacing(12)
        self.parameters = {}

    def add_scroll(self, route):
        scroll = ScrollArea()
        scroll.setObjectName(route)
        scroll.setWidgetResizable(True)
        scroll.setStyleSheet("QScrollArea { background: transparent; border: none; }")
        scroll.viewport().setStyleSheet("background: transparent;")
        view = QWidget()
        view.setObjectName(route + "_content")
        view.setStyleSheet("QWidget#" + view.objectName() + " { background: transparent; }")
        layout = QVBoxLayout(view)
        layout.setContentsMargins(0, 0, 0, 4)
        layout.setSpacing(12)
        layout.setAlignment(Qt.AlignTop)
        scroll.setWidget(view)
        return scroll, layout


class TabbedPage(ContentPage):
    def __init__(self, name, parent=None):
        super().__init__(name, parent)
        self.pivot = Pivot(self)
        self.stack = QStackedWidget(self)
        self.tabs = {}
        self.main_layout.addWidget(self.pivot)
        self.main_layout.addWidget(self.stack, 1)
        self.stack.currentChanged.connect(self._sync_tab)

    def add_tab(self, route, text):
        scroll, layout = self.add_scroll(route)
        self.tabs[route] = scroll
        self.stack.addWidget(scroll)
        self.pivot.addItem(routeKey=route, text=text,
                           onClick=lambda: self.stack.setCurrentWidget(scroll))
        if self.stack.count() == 1:
            self.pivot.setCurrentItem(route)
        return layout

    def _sync_tab(self, index):
        if index >= 0:
            self.pivot.setCurrentItem(self.stack.widget(index).objectName())


def card_columns(layout, *cards):
    row = QHBoxLayout()
    row.setSpacing(12)
    for card in cards:
        row.addWidget(card, 1, Qt.AlignTop)
    layout.addLayout(row)
