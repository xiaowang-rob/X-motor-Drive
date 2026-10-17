"""通讯页面：CAN 参数配置与解析器预留区域。"""
from app.ui_components import ContentPage, SectionCard


class CommunicationPage(ContentPage):
    def __init__(self):
        super().__init__("communication_page")
        self.scroll, layout = self.add_scroll("communication")
        self.main_layout.addWidget(self.scroll, 1)
        self.can_card = SectionCard("CAN 配置")
        self.can_card.add_fields([
            ("CAN ID", ""),
            ("CAN 模式", ["静默模式", "应答模式"]),
        ])
        layout.addWidget(self.can_card)
        self.can_parser_card = SectionCard("CAN 解析器")
        layout.addWidget(self.can_parser_card)
        self.bind_parameters()
