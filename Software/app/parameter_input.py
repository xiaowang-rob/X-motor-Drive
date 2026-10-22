"""可复用的参数行；只发布请求，由业务层负责协议和设备访问。"""
import math
import struct
from PyQt5.QtCore import pyqtSignal
from PyQt5.QtWidgets import QWidget, QHBoxLayout, QSizePolicy
from qfluentwidgets import BodyLabel, ComboBox, LineEdit, FluentIcon as FIF


VAL_TYPE_TO_UI = {
    "FLD_U8": "uint8",
    "FLD_U16": "uint16",
    "FLD_U32": "uint32",
    "FLD_F32": "float32",
    "FLD_BOOL": "bool",
}


class ParameterInput(QWidget):
    """统一参数行；label_width 设置标题及单位宽度，输入框自适应，按钮固定。"""
    DEFAULT_LABEL_WIDTH = 240
    EDITOR_MIN_WIDTH = 60

    read_requested = pyqtSignal(object)
    write_requested = pyqtSignal(object, object)
    default_requested = pyqtSignal(object)
    value_changed = pyqtSignal(object, object)
    validation_failed = pyqtSignal(object, str)

    def __init__(self, name, param_id, protocol_id=None, data_type="float32", unit="",
                 options=None, default=None, read_only=False, parent=None, *,
                 label_width=None, show_actions=True):
        super().__init__(parent)
        from app.ui_components import icon_action

        if data_type not in ("uint8", "uint16", "uint32", "int32", "float32", "bool", "str"):
            raise ValueError(f"不支持的参数类型：{data_type}")

        self.param_id = param_id
        self.protocol_id = param_id if protocol_id is None else protocol_id
        self.data_type = data_type
        self.default = default
        self.show_actions = show_actions

        if data_type == "bool" and options is None:
            options = [("否", False), ("是", True)]

        self.label = BodyLabel(f"{name}（{unit}）" if unit else name)
        self.set_label_width(self.DEFAULT_LABEL_WIDTH if label_width is None else label_width)
        self.label.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Preferred)
        self.editor = ComboBox() if options is not None else LineEdit()
        if options is not None:
            for index, item in enumerate(options):
                text, value = item if isinstance(item, tuple) else (item, index)
                self.editor.addItem(text, userData=self._convert(value))
        self.editor.setMinimumWidth(self.EDITOR_MIN_WIDTH)
        self.editor.setFixedHeight(32)
        self.editor.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        self.editor.setAccessibleName(self.label.text())
        self.label.setBuddy(self.editor)

        self.write_button = icon_action(FIF.UP, "写入参数")
        self.default_button = icon_action(FIF.SYNC, "恢复默认")
        self.read_button = icon_action(FIF.DOWNLOAD, "读取参数")
        row = QHBoxLayout(self)
        row.setContentsMargins(0, 0, 0, 0)
        row.setSpacing(6)
        row.addWidget(self.label)
        row.addWidget(self.editor, 1)
        for button in (self.write_button, self.default_button, self.read_button):
            button.setFixedSize(32, 32)
            button.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed)
            row.addWidget(button)
            button.setProperty("deviceAction", True)

        self.set_device_enabled(False)
        self.set_read_only(read_only)
        self.read_button.clicked.connect(
            lambda: self.read_requested.emit(self.protocol_id))
        self.write_button.clicked.connect(self._request_write)
        self.default_button.clicked.connect(self._request_default)
        if options is not None:
            self.editor.currentIndexChanged.connect(self._edited)
        else:
            self.editor.editingFinished.connect(self._edited)
        if default is not None:
            self.set_value(default)

    @classmethod
    def set_default_label_width(cls, width):
        """设置后续参数控件统一的标题列宽度，包含标题和单位。"""
        width = int(width)
        if width < 0:
            raise ValueError("标题宽度不能为负数")
        cls.DEFAULT_LABEL_WIDTH = width

    def set_label_width(self, width):
        """设置当前参数行的标题列宽度，输入框和按钮随剩余空间自适应。"""
        width = int(width)
        if width < 0:
            raise ValueError("标题宽度不能为负数")
        self.label.setFixedWidth(width)

    def _convert(self, value):
        if self.data_type == "str":
            return str(value)
        if self.data_type == "bool":
            if isinstance(value, str):
                text = value.strip().lower()
                if text in ("true", "1", "yes", "是"):
                    return True
                if text in ("false", "0", "no", "否"):
                    return False
                raise ValueError("请输入 true/false")
            return bool(value)
        if self.data_type == "float32":
            result = float(value)
            if not math.isfinite(result):
                raise ValueError("请输入有限数值")
            try:
                struct.pack("<f", result)
            except (OverflowError, struct.error) as error:
                raise ValueError("数值超出 float32 范围") from error
            return result
        result = int(str(value).strip())
        low, high = {
            "uint8": (0, 255),
            "uint16": (0, 65535),
            "uint32": (0, 4294967295),
            "int32": (-2147483648, 2147483647),
        }[self.data_type]
        if not low <= result <= high:
            raise ValueError(f"请输入 {low} 至 {high} 的整数")
        return result

    def value(self):
        if isinstance(self.editor, ComboBox):
            data = self.editor.currentData()
            if data is None:
                raise ValueError("请选择有效选项")
            return self._convert(data)
        return self._convert(self.editor.text())

    def set_value(self, value):
        """回填数据，不发出写入请求或 value_changed。"""
        value = self._convert(value)
        if isinstance(self.editor, ComboBox):
            index = self.editor.findData(value)
            if index < 0:
                raise ValueError(f"未知选项：{value}")
            blocked = self.editor.blockSignals(True)
            self.editor.setCurrentIndex(index)
            self.editor.blockSignals(blocked)
        else:
            self.editor.setText(str(value))

    def _edited(self, *_):
        try:
            value = self.value()
        except (ValueError, TypeError) as error:
            self.editor.setToolTip(str(error))
            self.validation_failed.emit(self.protocol_id, str(error))
            return
        self.editor.setToolTip("")
        self.value_changed.emit(self.protocol_id, value)

    def _request_write(self):
        try:
            self.write_requested.emit(self.protocol_id, self.value())
        except (ValueError, TypeError) as error:
            self.editor.setToolTip(str(error))
            self.validation_failed.emit(self.protocol_id, str(error))

    def _request_default(self):
        # 默认值必须来自业务配置，未知时请求业务层回填，不杜撰设备默认值。
        if self.default is not None:
            self.set_value(self.default)
            self.value_changed.emit(self.protocol_id, self.value())
        self.default_requested.emit(self.protocol_id)

    def set_read_only(self, read_only):
        self.read_only = read_only
        if isinstance(self.editor, LineEdit):
            self.editor.setReadOnly(read_only)
        else:
            self.editor.setEnabled(not read_only)
        visible = not read_only and self.show_actions
        for button in (self.write_button, self.default_button, self.read_button):
            button.setVisible(visible)

    def set_device_enabled(self, enabled):
        for button in (self.read_button, self.write_button, self.default_button):
            button.setEnabled(enabled)
