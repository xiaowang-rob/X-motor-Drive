"""单页固件维护 UI。"""
from PyQt5.QtWidgets import QHBoxLayout
from qfluentwidgets import LineEdit, ProgressBar, PushButton
from app.ui_components import ContentPage, SectionCard, button_row


class IAPPage(ContentPage):
    def __init__(self):
        super().__init__("iap_page")
        self.scroll, content = self.add_scroll("firmware_content")
        self.main_layout.addWidget(self.scroll)

        info_row = QHBoxLayout()
        info_row.setSpacing(12)
        self.device_card = SectionCard("设备版本信息")
        self.device_card.add_fields([
            ("设备型号", ""),
            ("Bootloader 版本", ""),
            ("Application 版本", ""),
        ], read_only=True, data_type="str")
        info_row.addWidget(self.device_card, 1)

        self.driver_card = SectionCard("驱动器信息")
        self.driver_card.add_fields([
            ("额定电压", "V"),
            ("额定电流", "A"),
            ("极对数", ""),
            ("相电阻", "Ω"),
            ("d 轴电感", "mH"),
            ("q 轴电感", "mH"),
        ], read_only=True)
        info_row.addWidget(self.driver_card, 1)
        content.addLayout(info_row)

        row = QHBoxLayout()
        row.setSpacing(12)
        self.firmware_panels = {}
        for route, title in [("bootloader", "Bootloader 固件"), ("application", "Application 固件")]:
            card = SectionCard(title)
            path = LineEdit()
            path.setReadOnly(True)
            path.setFixedHeight(32)
            path.setAccessibleName(title + "文件路径")
            path.setPlaceholderText("选择固件文件")
            card.content.addWidget(path)
            card.content.addLayout(button_row("选择文件", "烧录", "取消"))
            progress = ProgressBar()
            progress.setRange(0, 100)
            progress.setValue(0)
            progress.setFixedHeight(4)
            progress.setAccessibleName(title + "烧录进度")
            card.content.addWidget(progress)
            row.addWidget(card, 1)
            self.firmware_panels[route] = card
        content.addLayout(row)
        self.bind_parameters()
        for route, card in self.firmware_panels.items():
            buttons = card.findChildren(PushButton)
            for button in buttons:
                self.bind_action(button, f"firmware_{route}_{button.text()}")

    def set_device_info(self, values):
        for name, value in values.items():
            if name in self.device_card.parameters:
                self.device_card.parameters[name].set_value(value)

    def set_driver_info(self, values):
        for name, value in values.items():
            if name in self.driver_card.parameters:
                self.driver_card.parameters[name].set_value(value)

    def set_progress(self, route, value):
        card = self.firmware_panels[route]
        card.findChildren(ProgressBar)[0].setValue(int(value))
