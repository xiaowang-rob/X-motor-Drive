"""下位机日志显示，数据源留待业务阶段接入。"""
from qfluentwidgets import PlainTextEdit
from app.ui_components import ContentPage, style_console


class TerminalPage(ContentPage):
    def __init__(self):
        super().__init__("log_page")
        self.output = PlainTextEdit()
        self.output.setReadOnly(True)
        style_console(self.output)
        self.output.setAccessibleName("下位机日志输出")
        self.output.document().setMaximumBlockCount(10000)
        self.main_layout.addWidget(self.output, 1)
        self.bind_parameters()

    def append_log(self, text):
        self.output.appendPlainText(str(text))

    def set_log(self, text):
        self.output.setPlainText(str(text))

    def clear_log(self):
        self.output.clear()
