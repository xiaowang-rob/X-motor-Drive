"""校准卡片：n 个协议参数 + n 个校准指令按钮 + help 说明弹窗。"""
from dataclasses import dataclass

from PyQt5.QtCore import pyqtSignal
from PyQt5.QtWidgets import QHBoxLayout
from qfluentwidgets import FluentIcon as FIF, MessageBox

from protocol import PARAMETERS
from app.parameter_input import ParameterInput, VAL_TYPE_TO_UI
from app.ui_components import PlainCard, icon_action, preview_button


@dataclass(frozen=True)
class CalibrationSpec:
    """一组校准配置；参数和指令都由 protocol 中的常量引用。"""
    scope: str
    fields: tuple = ()
    actions: tuple = ()
    help_text: str = ""


class CalibrationCard(PlainCard):
    """固定高度的校准卡片，参数与指令处于同一行，help 按钮始终可用。"""
    action_requested = pyqtSignal(str, object)

    def __init__(self, spec: CalibrationSpec, parent=None):
        super().__init__(spec.scope, parent)
        self.spec = spec

        row = QHBoxLayout()
        row.setContentsMargins(0, 0, 0, 0)
        row.setSpacing(8)

        for param_id in spec.fields:
            meta = PARAMETERS[param_id]
            parameter = ParameterInput(
                meta.name,
                f"{spec.scope}/{meta.name}",
                protocol_id=meta.id,
                data_type=VAL_TYPE_TO_UI[meta.val_type],
                unit=meta.unit,
                options=meta.options,
                default=meta.default,
                label_width=120,
            )
            row.addWidget(parameter, 1)
            self.parameters[meta.name] = parameter
            self.fields[meta.name] = parameter.editor

        for command, text in spec.actions:
            button = preview_button(text)
            button.setIcon(FIF.UPDATE)
            button.setProperty("calibrationScope", spec.scope)
            button.setProperty("calibrationCommand", command)
            button.clicked.connect(lambda _=False, b=button: self._emit_action(b))
            row.addWidget(button)

        self.help_button = icon_action(FIF.INFO, f"{spec.scope}说明")
        self.help_button.clicked.connect(self._show_help)
        row.addWidget(self.help_button)
        self.content.addLayout(row)

    def _emit_action(self, button):
        try:
            params = {p.protocol_id: p.value() for p in self.parameters.values()}
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
                "command": button.property("calibrationCommand"),
                "operation": button.text(),
                "params": params,
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
