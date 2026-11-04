"""固件维护 UI：设备信息、驱动信息终端、Application 固件烧录入口。"""
from PyQt5.QtWidgets import QHBoxLayout, QVBoxLayout, QWidget
from qfluentwidgets import (
    FluentIcon as FIF, LineEdit, PlainTextEdit, ProgressBar, PushButton,
)
from app.ui_components import (
    ContentPage, PlainCard, icon_action, preview_button, style_console,
)


class IAPPage(ContentPage):
    def __init__(self):
        super().__init__("iap_page")
        self.scroll, content = self.add_scroll("firmware_content")
        self.main_layout.addWidget(self.scroll)

        # 左列：设备版本信息 + 固件选择/烧录，纵向排列成一条窄栏。
        left = QVBoxLayout()
        left.setContentsMargins(0, 0, 0, 0)
        left.setSpacing(12)

        self.device_card = PlainCard("设备版本信息")
        self.device_card.add_fields([
            ("设备型号", ""),
            ("Bootloader 版本", ""),
            ("Application 版本", ""),
        ], read_only=True, data_type="str", show_actions=False)
        left.addWidget(self.device_card)

        self.firmware_panels = {}
        self.firmware_select_buttons = {}
        self.firmware_paths = {}
        # 暂只支持烧录应用固件；Bootloader 烧录入口先不提供，
        # 需要时在此追加 ("bootloader", "Bootloader 固件") 即可恢复。
        for route, title in [("application", "Application 固件")]:
            card = PlainCard(title)
            path = LineEdit()
            path.setReadOnly(True)
            path.setFixedHeight(32)
            path.setAccessibleName(title + "文件路径")
            path.setPlaceholderText("选择固件文件")
            select_button = icon_action(FIF.FOLDER, "选择固件文件")
            path_row = QHBoxLayout()
            path_row.setContentsMargins(0, 0, 0, 0)
            path_row.setSpacing(6)
            path_row.addWidget(path, 1)
            path_row.addWidget(select_button)
            card.content.addLayout(path_row)
            # 烧录/取消左右等宽，占满整行。
            action_row = QHBoxLayout()
            action_row.setContentsMargins(0, 0, 0, 0)
            action_row.setSpacing(6)
            for text in ("烧录", "取消"):
                action_row.addWidget(preview_button(text, primary=text == "烧录"), 1)
            card.content.addLayout(action_row)
            progress = ProgressBar()
            progress.setRange(0, 100)
            progress.setValue(0)
            progress.setFixedHeight(4)
            progress.setAccessibleName(title + "烧录进度")
            card.content.addWidget(progress)
            left.addWidget(card)
            self.firmware_panels[route] = card
            self.firmware_select_buttons[route] = select_button
            self.firmware_paths[route] = path

        left.addStretch(1)
        left_column = QWidget()
        left_column.setLayout(left)

        # 右列：驱动器信息终端，占满剩余宽度与高度。
        self.driver_card = PlainCard("驱动器信息")
        self.driver_output = PlainTextEdit()
        self.driver_output.setReadOnly(True)
        style_console(self.driver_output)
        self.driver_output.setAccessibleName("驱动器信息输出")
        self.driver_output.setToolTip("驱动器信息输出")
        self.driver_output.document().setMaximumBlockCount(500)
        self.driver_output.setMinimumHeight(240)
        self.driver_card.content.addWidget(self.driver_output, 1)

        body = QHBoxLayout()
        body.setSpacing(12)
        # 左右两列等宽。
        body.addWidget(left_column, 1)
        body.addWidget(self.driver_card, 1)
        content.addLayout(body, 1)

        self.bind_parameters()
        for route, card in self.firmware_panels.items():
            self.bind_action(self.firmware_select_buttons[route], f"firmware_{route}_select")
            for button in card.findChildren(PushButton):
                self.bind_action(button, f"firmware_{route}_{button.text()}")

    def set_device_info(self, values):
        for name, value in values.items():
            if name in self.device_card.parameters:
                self.device_card.parameters[name].set_value(value)

    def append_driver_log(self, text):
        self.driver_output.appendPlainText(str(text))

    def set_driver_log(self, text):
        self.driver_output.setPlainText(str(text))

    def clear_driver_log(self):
        self.driver_output.clear()

    def set_driver_info(self, values):
        self.set_driver_log("\n".join(f"{name}: {value}" for name, value in values.items()))

    def set_progress(self, route, value):
        card = self.firmware_panels[route]
        card.findChildren(ProgressBar)[0].setValue(int(value))
