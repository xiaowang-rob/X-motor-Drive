"""监听页面：CAN / RS485 两个空页，内容后续接入。"""
from app.ui_components import TabbedPage, PlainCard


class MonitorPage(TabbedPage):
    def __init__(self):
        super().__init__("monitor_page")
        for route, title in (("can", "CAN"), ("rs485", "485")):
            layout = self.add_tab(route, title)
            layout.addWidget(PlainCard(title), 1)
        self.bind_parameters()
