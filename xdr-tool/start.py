"""XDr 电机驱动上位机 —— 纯 UI 入口（不初始化设备业务）。"""

import sys
from PyQt5.QtWidgets import QApplication

from app.main_window import MainWindow
from app.cyber_splash import show_splash

if __name__ == "__main__":
    app = QApplication(sys.argv)

    main_window = MainWindow()

    app.splash = show_splash(main_window)

    sys.exit(app.exec_())
