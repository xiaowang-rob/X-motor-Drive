"""XDr 电机驱动上位机 —— 纯 UI 入口（不初始化设备业务）。"""
import sys
from PyQt5.QtWidgets import QApplication

from app.main_window import MainWindow


if __name__ == '__main__':
    app = QApplication(sys.argv)

    main_window = MainWindow()
    main_window.show()
    sys.exit(app.exec_())
