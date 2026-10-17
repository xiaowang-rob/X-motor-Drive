"""纯 UI 回归测试：QT_QPA_PLATFORM=offscreen python -m unittest test_ui -v。"""
import os
import sys
import unittest
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
from PyQt5.QtCore import Qt
from PyQt5.QtTest import QTest
from PyQt5.QtWidgets import QApplication, QSizePolicy
from qfluentwidgets import isDarkTheme, PushButton, Theme, setTheme, TitleLabel, Pivot
from qfluentwidgets.components.navigation import SegmentedWidget
from app.main_window import MainWindow
from app.ui_components import SectionCard, SurfaceCard, CompactComboBox
from app.parameter_input import ParameterInput


class UITest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])
        cls.window = MainWindow()
        cls.window.show()
        cls.app.processEvents()

    @classmethod
    def tearDownClass(cls):
        cls.window.close()
        cls.window.deleteLater()
        cls.app.processEvents()

    def pages(self):
        w = self.window
        return [w.config_page, w.communication_page, w.control_page, w.download_page, w.terminal_page]

    def test_navigation_and_single_pages(self):
        w = self.window
        self.assertEqual(w.stackedWidget.count(), 5)
        for page in self.pages():
            item = w.navigationInterface.widget(page.objectName())
            QTest.mouseClick(item.itemWidget, Qt.LeftButton)
            QTest.qWait(260)
            self.assertIs(w.stackedWidget.currentWidget(), page)
            self.assertTrue(w.top_area.isVisible())
            self.assertEqual(page.findChildren(TitleLabel), [])
        for page in self.pages()[1:]:
            self.assertEqual(page.findChildren(Pivot), [])
        self.assertEqual(w.terminal_page.objectName(), "log_page")
        config = w.config_page
        self.assertEqual(config.stack.count(), 5)
        self.assertNotIn("communication", config.tabs)
        for route, tab in config.tabs.items():
            config.pivot.items[route].click()
            self.assertIs(config.stack.currentWidget(), tab)

    def test_configuration_structure(self):
        page = self.window.config_page
        self.assertIsInstance(page.observer_nav.nav, SegmentedWidget)
        self.assertIsInstance(page.controller_nav.nav, SegmentedWidget)
        self.assertIsInstance(page.protection_nav.nav, SegmentedWidget)
        motor_cards = page.tabs["motor"].findChildren(SectionCard)
        self.assertGreaterEqual(len(motor_cards), 2)
        self.assertIn("相电阻(Rs)", page.parameter_card.fields)
        self.assertIn("磁链(Psif)", page.parameter_card.fields)
        self.assertEqual(page.motor_nav.stack.count(), 3)
        self.assertEqual(len(page.motor_calibration_row.findChildren(ParameterInput)), 2)
        self.assertFalse(self.window.communication_page.can_parser_card.fields)
        self.assertIs(page.motor_calibration_row.parent(), page.motor_nav.parent())
        requests = []
        def record_action(action, payload):
            requests.append((action, payload))
        page.action_requested.connect(record_action)
        try:
            for row, texts in (
                    (page.motor_calibration_row, ["RL校准", "Psif校准", "机械校准"]),
                    (page.observer_calibration_row, ["开始校准", "设置零位"]),
                    (page.controller_calibration_row, ["PID校准", "MIT校准"])):
                buttons = row.findChildren(PushButton)
                self.assertEqual([button.text() for button in buttons], texts)
                for button in buttons:
                    self.assertFalse(button.icon().isNull())
                    self.assertFalse(button.isEnabled())
                    button.clicked.emit()
                    self.assertEqual(requests[-1], ("calibrate", {
                        "section": button.property("calibrationScope"),
                        "operation": button.text()}))
        finally:
            page.action_requested.disconnect(record_action)
        communication = self.window.communication_page
        self.assertEqual(set(communication.can_card.fields), {"CAN ID", "CAN 模式"})
        self.assertEqual(communication.can_card.fields["CAN 模式"].count(), 2)
        communication.set_parameter("CAN 配置/CAN ID", 12)
        self.assertEqual(communication.get_parameter("CAN 配置/CAN ID"), 12)
        self.assertNotIn("CAN 配置/CAN ID", page.parameters)
        self.assertEqual(len(page.action_buttons), 7)
        self.assertEqual(page.config_file.width(), 36)
        self.assertEqual(page.action_panel.width(), 60)
        self.assertIn(page.config_file.currentText(), page.config_file.toolTip())
        self.app.processEvents()
        for button in page.action_buttons:
            self.assertEqual(button.width(), page.config_file.width())
            self.assertEqual(button.x(), page.config_file.x())
            self.assertTrue(button.toolTip())
            self.assertEqual(button.toolTip(), button.accessibleName())
            self.assertGreaterEqual(button.receivers(button.clicked), 1)

        parameter = page.parameter_card.parameters["相电阻(Rs)"]
        self.assertIsInstance(parameter, ParameterInput)
        self.assertEqual(parameter.label.text(), "相电阻(Rs)（Ω）")
        self.assertEqual(parameter.label.width(), ParameterInput.DEFAULT_LABEL_WIDTH)
        self.assertGreaterEqual(parameter.editor.minimumWidth(), ParameterInput.EDITOR_MIN_WIDTH)
        self.assertEqual(parameter.editor.sizePolicy().horizontalPolicy(), QSizePolicy.Expanding)
        parameter.set_label_width(210)
        self.assertEqual(parameter.label.width(), 210)
        for button in (parameter.write_button, parameter.default_button, parameter.read_button):
            self.assertEqual(button.width(), button.height())
            self.assertEqual(button.width(), 32)
        self.assertEqual(parameter.write_button.toolTip(), "写入参数")
        self.assertEqual(parameter.default_button.toolTip(), "恢复默认")
        self.assertEqual(parameter.read_button.toolTip(), "读取参数")

    def test_pmsm_scroll_is_local(self):
        w = self.window
        page = w.config_page
        w.resize(1180, 800)
        w.switchTo(page)
        page.stack.setCurrentWidget(page.tabs["motor"])
        page.motor_nav.stack.setCurrentWidget(page.pmsm_scroll)
        QTest.qWait(260)
        self.app.processEvents()
        self.assertGreater(page.pmsm_scroll.verticalScrollBar().maximum(), 0)
        self.assertEqual(page.tabs["motor"].verticalScrollBar().maximum(), 0)
        self.assertEqual(page.pmsm_scroll.horizontalScrollBar().maximum(), 0)
        nav_pos = page.motor_nav.nav.mapTo(page, page.motor_nav.nav.rect().topLeft())
        calibration_pos = page.motor_calibration_row.mapTo(page, page.motor_calibration_row.rect().topLeft())
        page.pmsm_scroll.verticalScrollBar().setValue(page.pmsm_scroll.verticalScrollBar().maximum())
        self.app.processEvents()
        self.assertEqual(page.motor_nav.nav.mapTo(page, page.motor_nav.nav.rect().topLeft()), nav_pos)
        self.assertEqual(page.motor_calibration_row.mapTo(page, page.motor_calibration_row.rect().topLeft()), calibration_pos)
        page.motor_nav.stack.setCurrentIndex(0)
        w.resize(1440, 960)

    def test_parameter_constructor_label_width(self):
        parameter = ParameterInput("电流", "test/current", unit="A", label_width=92)
        self.assertEqual(parameter.label.text(), "电流（A）")
        self.assertEqual(parameter.label.width(), 92)
        self.assertEqual(parameter.editor.sizePolicy().horizontalPolicy(), QSizePolicy.Expanding)
        for button in (parameter.write_button, parameter.default_button, parameter.read_button):
            self.assertEqual(button.size().width(), 32)
            self.assertEqual(button.size().height(), 32)
        parameter.deleteLater()
        with self.assertRaises(ValueError):
            ParameterInput("电流", "test/invalid", label_width=-1)

    def test_segmented_navigation_round_trip(self):
        page = self.window.config_page
        nav = page.controller_nav.nav
        pid = page.controller_nav.stack.widget(0)
        mit = page.controller_nav.stack.widget(1)
        nav.setCurrentItem("side_1")
        self.app.processEvents()
        self.assertIs(page.controller_nav.stack.currentWidget(), mit)
        self.assertEqual(nav.currentRouteKey(), "side_1")
        nav.setCurrentItem("side_0")
        self.app.processEvents()
        self.assertIs(page.controller_nav.stack.currentWidget(), pid)
        self.assertEqual(nav.currentRouteKey(), "side_0")

    def test_theme_and_local_controls(self):
        setTheme(Theme.DARK, save=False)
        QTest.mouseClick(self.window.top_area.theme_toggle, Qt.LeftButton)
        self.assertFalse(isDarkTheme())
        QTest.mouseClick(self.window.top_area.theme_toggle, Qt.LeftButton)
        self.assertTrue(isDarkTheme())

    def test_visual_consistency(self):
        w = self.window
        combo = w.config_page.config_file
        self.assertIsInstance(combo, CompactComboBox)
        w.switchTo(w.config_page)
        QTest.qWait(260)
        QTest.mouseClick(combo, Qt.LeftButton)
        self.app.processEvents()
        self.assertIsNotNone(combo.dropMenu)
        self.assertTrue(combo.dropMenu.isVisible())
        self.assertEqual(combo.dropMenu.actions()[0].text(), combo.currentText())
        combo.dropMenu.close()
        for card in w.findChildren(SurfaceCard):
            self.assertEqual(card.borderRadius, 8)
            self.assertEqual(card._hoverBackgroundColor(), card._normalBackgroundColor())
        for theme, background in [(Theme.LIGHT, "#f5f7fa"), (Theme.DARK, "#191d24")]:
            setTheme(theme, save=False)
            for page in self.pages():
                w.switchTo(page)
                QTest.qWait(260)
                self.assertFalse(page.grab().isNull())
            for editor in (w.top_area.host_output, w.terminal_page.output):
                self.assertIn(background, editor.styleSheet())
                self.assertTrue(editor.tabChangesFocus())
            for card in w.findChildren(SectionCard):
                for field in card.fields.values():
                    self.assertEqual(field.height(), 32)
        w.resize(1180, 800)
        w.switchTo(w.control_page)
        QTest.qWait(260)
        page = w.control_page
        widgets = [page.control_mode, *page.action_buttons, page.send_target]
        previous = None
        for widget in widgets:
            rect = widget.rect().translated(widget.mapTo(page.control_card, widget.rect().topLeft()))
            self.assertTrue(page.control_card.rect().contains(rect), widget.accessibleName())
            self.assertEqual(widget.height(), 32)
            if previous is not None:
                self.assertLess(previous.right(), rect.left())
            previous = rect
        for control in page.target_controls:
            self.assertGreaterEqual(control.slider.width(), 120)
        w.resize(1440, 960)

    def test_target_ranges(self):
        page = self.window.control_page
        for control in page.target_controls:
            self.assertEqual(control.minimum.text(), "0")
            self.assertEqual(control.minimum.accessibleName(), control.target.accessibleName().replace("目标值", "最小值"))
            for end in (10, -10):
                control.maximum.setText(str(end))
                control.maximum.editingFinished.emit()
                control.slider.setValue(5000)
                self.assertAlmostEqual(float(control.target.text()), end / 2)
                control.target.setText(str(end / 4))
                control.target.editingFinished.emit()
                self.assertEqual(control.slider.value(), 2500)
                control.slider.setValue(10000)
                self.assertAlmostEqual(float(control.target.text()), end)
            control.maximum.setText("0")
            control.maximum.editingFinished.emit()
            self.assertFalse(control.slider.isEnabled())
            self.assertEqual(float(control.target.text()), 0)
            control.maximum.setText("10")
            control.maximum.editingFinished.emit()
            control.slider.setValue(5000)
            self.assertEqual(float(control.target.text()), 5)
            control.maximum.setText("10")
        self.assertIn("A", page.targets[2].toolTip())
        self.assertEqual(page.control_mode.itemText(2), "电流控制")
        self.assertEqual(page.control_toolbar.itemAt(page.control_toolbar.count() - 1).widget(), page.send_target)
        self.assertEqual(page.channel_card.width(), 192)
        for i, badge in enumerate(page.channel_badges, 1):
            self.assertEqual(badge.text(), "")
            self.assertEqual(badge.height(), 3)
            self.assertIn(page.wave_canvas.channel_colors[i - 1], badge.styleSheet())

    def test_page_and_status_api(self):
        w = self.window
        status = w.top_area
        status.update_status({"voltage": "48.0 V", "state": "运行"})
        self.assertEqual(status.indicators["voltage"].text(), "48.0 V")
        self.assertEqual(status.indicators["state"].text(), "运行")
        w.config_page.set_parameter("FOC 整定/相电阻", 1.25)
        self.assertEqual(w.config_page.get_parameter("FOC 整定/相电阻"), 1.25)
        self.assertIn("FOC 整定/相电阻", w.config_page.parameters)
        w.control_page.set_targets({"位置": "2.5"})
        self.assertEqual(w.control_page.targets[0].text(), "2.50")
        w.terminal_page.set_log("test")
        self.assertEqual(w.terminal_page.output.toPlainText(), "test")

    def test_output_and_business_isolation(self):
        w = self.window
        for output in (w.top_area.host_output, w.terminal_page.output):
            self.assertTrue(output.isReadOnly())
            self.assertEqual(output.toPlainText(), "")
        self.assertFalse(any(name in sys.modules for name in [
            "functions.com_port", "functions.wave", "functions.IAP_downloader", "protocol",
        ]))
        for button in w.findChildren(PushButton):
            if button.property("deviceAction"):
                self.assertFalse(button.isEnabled())
        self.assertEqual(set(w.download_page.device_card.fields), {
            "设备型号", "Bootloader 版本", "Application 版本",
        })
        self.assertEqual(set(w.download_page.driver_card.fields), {
            "额定电压", "额定电流", "极对数", "相电阻", "d 轴电感", "q 轴电感",
        })
        self.assertEqual(len(w.download_page.firmware_panels), 2)

    def test_geometry(self):
        w = self.window
        for width, height in [(1180, 800), (1440, 960)]:
            w.resize(width, height)
            for page in self.pages():
                w.switchTo(page)
                QTest.qWait(260)
                self.app.processEvents()
                top = w.top_area
                card_pos = top.card.mapTo(w, top.card.rect().topLeft())
                pos = page.mapTo(w, page.rect().topLeft())
                self.assertEqual(card_pos.x(), pos.x())
                self.assertEqual(top.card.width(), page.width())
                gap = pos.y() - card_pos.y() - top.card.height()
                self.assertTrue(24 <= gap <= 28)
                self.assertLessEqual(top.height(), 130)
                for widget in [top.com_port, top.connect_but, top.host_output,
                               top.system_reset_button, top.reset_button, top.theme_toggle]:
                    rect = widget.rect().translated(widget.mapTo(top.card, widget.rect().topLeft()))
                    self.assertTrue(top.card.rect().contains(rect), widget.accessibleName())
                scrolls = list(page.tabs.values()) if page is w.config_page else ([page.scroll] if page in (w.communication_page, w.control_page, w.download_page) else [])
                for scroll in scrolls:
                    if page is w.config_page:
                        page.stack.setCurrentWidget(scroll)
                    self.app.processEvents()
                    self.assertEqual(scroll.horizontalScrollBar().maximum(), 0, scroll.objectName())
            config = w.config_page
            self.assertLess(config.stack.mapTo(w, config.stack.rect().topRight()).x(),
                            config.action_panel.mapTo(w, config.action_panel.rect().topLeft()).x())
        w.resize(1440, 960)


if __name__ == "__main__":
    unittest.main()
