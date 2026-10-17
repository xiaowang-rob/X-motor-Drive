"""XDr 纯 UI 主窗口，设备业务暂不接入。"""
from pathlib import Path
from PyQt5.QtGui import QIcon, QColor
from PyQt5.QtWidgets import QWidget, QVBoxLayout
from qfluentwidgets import (
    FluentWindow, FluentIcon as FIF, Theme, setTheme, setThemeColor,
)
from app.status_bar import StatusBar
from app.config_page import ConfigPage
from app.communication_page import CommunicationPage
from app.control_page import ControlPage
from app.iap_page import IAPPage
from app.terminal_page import TerminalPage


class MainWindow(FluentWindow):
    def __init__(self):
        setTheme(Theme.DARK, save=False)
        setThemeColor(QColor("#4f9cf9"), save=False)
        super().__init__()
        self.setWindowTitle("XDr · Tool")
        self.setWindowIcon(QIcon(str(Path(__file__).parent / "images" / "logo.png")))
        self.resize(1440, 960)
        self.setMinimumSize(1180, 800)

        self.config_page = ConfigPage()
        self.communication_page = CommunicationPage()
        self.control_page = ControlPage()
        self.download_page = IAPPage()
        self.terminal_page = TerminalPage()
        self.top_area = StatusBar(self)
        self._embed_status_bar()
        for page, icon, title in [
            (self.config_page, FIF.SETTING, "配置"),
            (self.communication_page, FIF.CHAT, "通讯"),
            (self.control_page, FIF.SPEED_HIGH, "控制"),
            (self.download_page, FIF.DOWNLOAD, "固件"),
            (self.terminal_page, FIF.COMMAND_PROMPT, "日志"),
        ]:
            self.addSubInterface(page, icon, title)
        self.navigationInterface.setExpandWidth(176)
        self.navigationInterface.setCurrentItem(self.config_page.objectName())
        self.switchTo(self.config_page)

    def _embed_status_bar(self):
        self.widgetLayout.removeWidget(self.stackedWidget)
        container = QWidget(self)
        layout = QVBoxLayout(container)
        # 顶部与页面共享同一条左右边界，间距由外层统一管理。
        layout.setContentsMargins(24, 12, 24, 0)
        layout.setSpacing(24)
        self.stackedWidget.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self.top_area)
        layout.addWidget(self.stackedWidget, 1)
        self.widgetLayout.addWidget(container)
