"""日志页面：下位机 printf 与上位机、系统消息共用同一输出区。

其他模块往日志页写内容时统一调用 ``append_log(text, source=...)``；
新增一种来源只需在 ``LOG_PREFIXES`` 里加一项，界面自动带上前缀，
不用改动页面本身。
"""

from qfluentwidgets import PlainTextEdit

from app.ui_components import ContentPage, style_console

# 来源 -> 行前缀。空字符串表示原样输出：下位机 printf 保持既有观感，
# 其余来源带前缀以便与下位机数据区分。
LOG_PREFIXES = {
    "device": "",
    "host": "上位机",
    "system": "系统",
}


class TerminalPage(ContentPage):
    def __init__(self):
        super().__init__("log_page")
        self.output = PlainTextEdit()
        self.output.setReadOnly(True)
        style_console(self.output)
        self.output.setAccessibleName("日志输出")
        self.output.document().setMaximumBlockCount(10000)
        self.main_layout.addWidget(self.output, 1)
        self.bind_parameters()

    def append_log(self, text, source="device"):
        """统一日志入口。

        text 为任意可转字符串的内容；source 取自 LOG_PREFIXES，
        未登记的来源会直接作为前缀显示，便于临时接入新数据。
        """
        prefix = LOG_PREFIXES.get(source, source)
        line = str(text)
        if prefix:
            line = f"[{prefix}] {line}"
        self.output.appendPlainText(line)

    def append_host_log(self, text):
        """上位机的收发、配置等消息。"""
        self.append_log(text, source="host")

    def append_system_log(self, text):
        """升级、连接状态等系统级提示。"""
        self.append_log(text, source="system")

    def set_log(self, text):
        self.output.setPlainText(str(text))

    def clear_log(self):
        self.output.clear()
