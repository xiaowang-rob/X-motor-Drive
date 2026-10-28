"""基于 QFluentWidgets 的对话框封装。"""

from qfluentwidgets import BodyLabel, LineEdit, SubtitleLabel
from qfluentwidgets.components.dialog_box import MessageBoxBase


class TextInputDialog(MessageBoxBase):
    """单行文本输入弹窗，替代 QInputDialog。"""

    def __init__(self, title, label, text="", parent=None):
        super().__init__(parent)
        self.titleLabel = SubtitleLabel(title, self)
        self.label = BodyLabel(label, self)
        self.edit = LineEdit(self)
        self.edit.setText(text)
        self.edit.setPlaceholderText(label)
        self.edit.setClearButtonEnabled(True)
        self.viewLayout.addWidget(self.titleLabel)
        self.viewLayout.addWidget(self.label)
        self.viewLayout.addWidget(self.edit)
        self.yesButton.setText("保存")
        self.cancelButton.setText("取消")
        self.widget.setMinimumWidth(360)
        self.edit.setFocus()
        self.edit.returnPressed.connect(self.yesButton.click)

    def validate(self):
        return bool(self.edit.text().strip())

    def value(self):
        return self.edit.text().strip()
