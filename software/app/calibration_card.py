"""校准卡片：独立电流/速度输入、校准指令按钮与 help 说明弹窗。"""

from dataclasses import dataclass
import math

from PyQt5.QtCore import pyqtSignal
from PyQt5.QtWidgets import QHBoxLayout
from qfluentwidgets import BodyLabel, LineEdit, FluentIcon as FIF, MessageBox

from app.ui_components import PlainCard, icon_action, preview_button


@dataclass(frozen=True)
class CalibrationSpec:
    """一组校准配置；输入仅供本次命令使用，不参与参数读写。"""

    scope: str
    with_inputs: bool = False
    actions: tuple = ()
    help_text: str = ""


class CalibrationCard(PlainCard):
    """输入框与指令处于同一行，help 按钮始终可用。"""

    action_requested = pyqtSignal(str, object)

    def __init__(self, spec: CalibrationSpec, parent=None):
        super().__init__(spec.scope, parent)
        self.spec = spec

        row = QHBoxLayout()
        row.setContentsMargins(0, 0, 0, 0)
        row.setSpacing(8)

        self.help_button = icon_action(FIF.INFO, f"{spec.scope}说明")
        self.help_button.clicked.connect(self._show_help)
        row.addWidget(self.help_button)

        self.inputs = {}
        if spec.with_inputs:
            for key, name, unit, default in (
                ("tune_current", "校准电流", "A", "0"),
                ("tune_vel", "校准速度", "rad/s", "0"),
            ):
                label = BodyLabel(f"{name}（{unit}）")
                editor = LineEdit()
                editor.setText(default)
                editor.setFixedHeight(32)
                editor.setMinimumWidth(80)
                editor.setAccessibleName(label.text())
                label.setBuddy(editor)
                row.addWidget(label)
                row.addWidget(editor, 1)
                self.inputs[key] = editor

        for command, text in spec.actions:
            button = preview_button(text)
            button.setIcon(FIF.UPDATE)
            button.setProperty("calibrationScope", spec.scope)
            button.setProperty("calibrationCommand", command)
            button.clicked.connect(lambda _=False, b=button: self._emit_action(b))
            row.addWidget(button)

        self.content.addLayout(row)

    def _emit_action(self, button):
        command = button.property("calibrationCommand")
        values = {}
        try:
            for key, editor in self.inputs.items():
                try:
                    value = float(editor.text().strip())
                except ValueError:
                    raise ValueError(
                        f"{editor.accessibleName()}请输入有效数值"
                    ) from None
                if not math.isfinite(value) or abs(value) > 3.4028234663852886e38:
                    raise ValueError(
                        f"{editor.accessibleName()}超出 float32 有限数范围"
                    )
                values[key] = value
        except (TypeError, ValueError) as error:
            self.action_requested.emit(
                "calibration_invalid",
                {"scope": self.spec.scope, "error": str(error)},
            )
            return

        self.action_requested.emit(
            "calibrate",
            {
                "scope": self.spec.scope,
                "command": command,
                "operation": button.text(),
                "values": values,
            },
        )

    def _show_help(self):
        # 以所在的电机/观测器/控制器等页面为父级，遮罩覆盖整个页面；
        # 若校准卡片尚未挂到页面，再退回到窗口。
        parent = self.parentWidget() or self.window()
        MessageBox(
            f"{self.spec.scope}说明",
            self.spec.help_text or "暂无说明。",
            parent,
        ).exec_()
