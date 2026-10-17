"""同屏显示波形与手动控制，不连接设备。"""
from PyQt5.QtCore import Qt, QRectF
from PyQt5.QtGui import QPainter, QPainterPath, QColor, QPen
from PyQt5.QtWidgets import QWidget, QHBoxLayout, QVBoxLayout, QLabel
from qfluentwidgets import (
    BodyLabel, ComboBox, Slider, LineEdit, CheckBox,
    HorizontalSeparator, isDarkTheme, qconfig, setCustomStyleSheet,
)
from app.ui_components import ContentPage, SectionCard, preview_button


CHANNEL_COLORS = ("#58cfc2", "#f5ae50", "#e77fa3", "#9cce70", "#b59af7")


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
    def __init__(self, name, unit, endpoint, parent=None):
        super().__init__(parent)
        row = QHBoxLayout(self)
        row.setContentsMargins(0, 0, 0, 0)
        row.setSpacing(8)
        label = BodyLabel(name)
        label.setFixedWidth(32)
        row.addWidget(label)
        self.minimum = QLabel("0")
        self.minimum.setFixedWidth(28)
        self.minimum.setAlignment(Qt.AlignCenter)
        self.minimum.setAccessibleName(name + "最小值")
        self.minimum.setToolTip("最小值")
        self.maximum = self._line_edit(name + "最大值", str(endpoint))
        self.maximum.setToolTip("最大值")
        self.target = self._line_edit(name + "目标值", "0")
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
        self.slider.valueChanged.connect(self._from_slider)
        self.target.editingFinished.connect(self._from_target)
        self.maximum.editingFinished.connect(self._update_range)
        self._update_range()

    @staticmethod
    def _line_edit(name, value):
        edit = LineEdit()
        edit.setText(value)
        edit.setFixedSize(132, 32)
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


class ControlPage(ContentPage):
    def __init__(self):
        super().__init__("control_page")
        self.scroll, content = self.add_scroll("control_content")
        self.main_layout.addWidget(self.scroll)
        row = QHBoxLayout()
        row.setSpacing(12)
        self.channel_card = SectionCard("采样通道")
        self.channel_card.setFixedWidth(192)
        self.wave_ch, self.channel_badges = [], []
        for i, color in enumerate(CHANNEL_COLORS, 1):
            channel = QVBoxLayout()
            channel.setContentsMargins(0, 0, 0, 0)
            channel.setSpacing(4)
            combo = ComboBox()
            combo.setMinimumWidth(0)
            combo.setFixedHeight(32)
            combo.addItems(["未选择", "位置", "速度", "电流", "Iq", "Id", "母线电压"])
            combo.setAccessibleName(f"通道 {i} 信号")
            line = QLabel()
            line.setFixedHeight(3)
            line.setAccessibleName(f"通道 {i} 颜色")
            line.setStyleSheet(f"background-color: {color}; border-radius: 1px;")
            channel.addWidget(combo)
            channel.addWidget(line)
            self.channel_card.content.addLayout(channel)
            self.wave_ch.append(combo)
            self.channel_badges.append(line)
        self.channel_card.content.addWidget(HorizontalSeparator())
        for text in ["自动 X 轴", "自动 Y 轴"]:
            check = CheckBox(text)
            check.setChecked(True)
            self.channel_card.content.addWidget(check)
        for text in ["开始采样", "清空"]:
            self.channel_card.content.addWidget(preview_button(text))
        row.addWidget(self.channel_card)
        plot = SectionCard("波形")
        self.wave_canvas = WaveCanvas()
        plot.content.addWidget(self.wave_canvas, 1)
        row.addWidget(plot, 1)
        content.addLayout(row)

        self.control_card = SectionCard("手动控制")
        self.control_toolbar = QHBoxLayout()
        self.control_toolbar.setSpacing(8)
        self.control_toolbar.addWidget(BodyLabel("控制模式"))
        self.control_mode = ComboBox()
        self.control_mode.addItems(["位置控制", "速度控制", "电流控制", "MIT 控制"])
        self.control_mode.setAccessibleName("控制模式")
        self.control_mode.setFixedSize(120, 32)
        self.control_toolbar.addWidget(self.control_mode)
        self.action_buttons = []
        for text in ["使能", "失能", "制动", "设置零点", "设置限位"]:
            button = preview_button(text)
            self.control_toolbar.addWidget(button)
            self.action_buttons.append(button)
        self.control_toolbar.addStretch()
        self.send_target = preview_button("发送目标", primary=True)
        self.control_toolbar.addWidget(self.send_target)
        self.control_card.content.addLayout(self.control_toolbar)
        self.control_card.content.addWidget(HorizontalSeparator())
        self.target_controls = []
        self.sliders, self.targets = [], []
        for name, unit, endpoint in [("位置", "rad", 180), ("速度", "rad/s", 100), ("电流", "A", 10)]:
            control = TargetControl(name, unit, endpoint)
            self.control_card.content.addWidget(control)
            self.target_controls.append(control)
            self.sliders.append(control.slider)
            self.targets.append(control.target)
        content.addWidget(self.control_card)
        self.bind_parameters()
        for button, action in zip(self.action_buttons, ("enable", "disable", "brake", "set_zero", "set_limit")):
            self.bind_action(button, action)
        self.bind_action(self.send_target, "send_target", self.get_targets)

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
            index = combo.findText(value)
            if index >= 0:
                combo.setCurrentIndex(index)
