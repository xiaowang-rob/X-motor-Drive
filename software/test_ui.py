"""纯 UI 回归测试：QT_QPA_PLATFORM=offscreen python -m unittest test_ui -v。"""
import os
import sys
import unittest
from unittest.mock import call, patch
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
import struct
from PyQt5.QtCore import Qt
from PyQt5.QtTest import QTest
from PyQt5.QtWidgets import QApplication, QSizePolicy
from qfluentwidgets import (
    isDarkTheme, PushButton, ToolButton, Theme, setTheme, TitleLabel, Pivot,
    PlainTextEdit, LineEdit, ProgressBar,
)
from qfluentwidgets.components.navigation import SegmentedWidget
from app.main_window import MainWindow
from app.ui_components import (
    SectionCard, PlainCard, ScrollCard, SurfaceCard, CompactComboBox,
)
from app.parameter_input import ParameterInput
from app.calibration_card import CalibrationCard
from app.config_page import COMMUNICATION_PARAMS, MOTOR_PARAMS, PROTECTION_PAGES
from app.control_page import (
    ControlPage, MODE_PAGE_MODES, WAVE_CHANNEL_COUNT, WAVE_CHANNELS,
)
from app.services import payload_codec as codec
from app.waveform_widget import WaveformWidget
from protocol import (
    CmdId, FbId, PARAMETERS, Pidx, Didx, OBSDATA, UartFrame, eMotorState,
)


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
        return [w.config_page, w.monitor_page, w.control_page, w.download_page, w.terminal_page]

    def test_navigation_and_pages(self):
        w = self.window
        self.assertEqual(w.stackedWidget.count(), 5)
        for page in self.pages():
            item = w.navigationInterface.widget(page.objectName())
            QTest.mouseClick(item.itemWidget, Qt.LeftButton)
            QTest.qWait(260)
            self.assertIs(w.stackedWidget.currentWidget(), page)
            self.assertTrue(w.top_area.isVisible())
            self.assertEqual(page.findChildren(TitleLabel), [])

        self.assertEqual(w.monitor_page.objectName(), "monitor_page")
        self.assertEqual(w.monitor_page.stack.count(), 2)
        self.assertEqual(set(w.monitor_page.tabs), {"can", "rs485"})
        self.assertIsInstance(w.monitor_page.pivot, Pivot)
        self.assertEqual(w.terminal_page.objectName(), "log_page")

        config = w.config_page
        self.assertEqual(config.stack.count(), 6)
        self.assertEqual(list(config.tabs), [
            "motor", "observer", "controller", "trajectory", "communication", "protection",
        ])
        for route, tab in config.tabs.items():
            config.pivot.items[route].click()
            self.assertIs(config.stack.currentWidget(), tab)

    def test_configuration_structure(self):
        page = self.window.config_page
        self.assertIsInstance(page.motor_scroll, ScrollCard)
        self.assertIsInstance(page.motor_calibration, CalibrationCard)
        self.assertIsInstance(page.observer_nav.nav, SegmentedWidget)
        self.assertIsInstance(page.controller_nav.nav, SegmentedWidget)
        self.assertIsInstance(page.protection_scroll, ScrollCard)
        self.assertEqual(page.observer_nav.stack.count(), 6)
        self.assertEqual(page.controller_nav.stack.count(), 3)
        self.assertEqual(
            set(page.protection_scroll.parameters),
            {PARAMETERS[param_id].name for param_id in PROTECTION_PAGES},
        )

        self.assertEqual(
            set(page.motor_scroll.parameters),
            {PARAMETERS[param_id].name for param_id in MOTOR_PARAMS},
        )

        self.assertEqual(
            set(page.motor_calibration.parameters),
            {
                PARAMETERS[Pidx.PARAM_TUNE_CURRENT].name,
                PARAMETERS[Pidx.PARAM_TUNE_VEL].name,
            },
        )
        self.assertEqual(
            {button.text() for button in page.motor_calibration.findChildren(PushButton)},
            {"RL校准", "Psif校准", "机械校准"},
        )
        self.assertIsInstance(page.motor_calibration.help_button, ToolButton)
        self.assertIn("电机校准", page.motor_calibration.spec.scope)
        self.assertIn("观测器校准", page.observer_calibration.spec.scope)
        self.assertEqual(page.pid_calibration.spec.actions[0][0], CmdId.CMD_PID_TUNNING)
        self.assertEqual(page.mit_calibration.spec.actions[0][0], CmdId.CMD_MIT_TUNNING)

        self.assertIsInstance(page.can_card, ScrollCard)
        self.assertEqual(
            set(page.can_card.parameters),
            {PARAMETERS[param_id].name for param_id in COMMUNICATION_PARAMS},
        )
        self.assertEqual(
            page.can_card.parameters[PARAMETERS[Pidx.PARAM_CAN_RATE].name].editor.count(), 2)
        self.assertIsInstance(page.rs485_card, PlainCard)
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

        parameter = page.parameters[Pidx.PARAM_MOTOR_RS]
        self.assertIsInstance(parameter, ParameterInput)
        self.assertEqual(parameter.protocol_id, Pidx.PARAM_MOTOR_RS)
        motor_rs = PARAMETERS[Pidx.PARAM_MOTOR_RS]
        expected_label = (
            f"{motor_rs.name}（{motor_rs.unit}）" if motor_rs.unit else motor_rs.name)
        self.assertEqual(parameter.label.text(), expected_label)
        self.assertEqual(parameter.label.width(), ParameterInput.DEFAULT_LABEL_WIDTH)
        self.assertGreaterEqual(parameter.editor.minimumWidth(), ParameterInput.EDITOR_MIN_WIDTH)
        self.assertEqual(parameter.editor.sizePolicy().horizontalPolicy(), QSizePolicy.Expanding)
        parameter.set_label_width(210)
        self.assertEqual(parameter.label.width(), 210)
        for button in (parameter.write_button, parameter.default_button, parameter.read_button):
            self.assertEqual(button.width(), button.height())
            self.assertEqual(button.width(), 32)
            self.assertEqual(button.toolTip(), {
                parameter.write_button: "写入参数",
                parameter.default_button: "恢复默认",
                parameter.read_button: "读取参数",
            }[button])

        self.assertEqual(page.parameters[Pidx.PARAM_ABI_COUNT].data_type, "uint16")
        self.assertEqual(page.parameters[Pidx.PARAM_POSITIVE_DIR].data_type, "bool")
        self.assertEqual(page.parameters[Pidx.PARAM_POSITIVE_DIR].editor.count(), 2)
        page.set_parameter(Pidx.PARAM_MOTOR_RS, 1.25)
        self.assertEqual(page.get_parameter(Pidx.PARAM_MOTOR_RS), 1.25)

        requests = []

        def record_action(action, payload):
            requests.append((action, payload))

        page.action_requested.connect(record_action)
        try:
            page.set_device_enabled(True)
            button = next(
                button for button in page.motor_calibration.findChildren(PushButton)
                if button.text() == "RL校准"
            )
            button.clicked.emit()
            self.assertEqual(requests[-1][0], "calibrate")
            payload = requests[-1][1]
            self.assertEqual(payload["scope"], "电机校准")
            self.assertEqual(payload["command"], CmdId.CMD_RL_TUNNING)
            self.assertIn(Pidx.PARAM_TUNE_CURRENT, payload["params"])
            self.assertIn(Pidx.PARAM_TUNE_VEL, payload["params"])
        finally:
            page.action_requested.disconnect(record_action)
            page.set_device_enabled(True)

    def test_scroll_card_is_local(self):
        w = self.window
        page = w.config_page
        w.resize(1180, 800)
        w.switchTo(page)
        page.stack.setCurrentWidget(page.tabs["motor"])
        QTest.qWait(260)
        self.app.processEvents()
        self.assertGreater(page.motor_scroll.scroll.verticalScrollBar().maximum(), 0)
        self.assertEqual(page.motor_scroll.scroll.horizontalScrollBar().maximum(), 0)
        parent = page.tabs["motor"]
        calibration_pos = page.motor_calibration.mapTo(
            parent, page.motor_calibration.rect().topLeft())
        page.motor_scroll.scroll.verticalScrollBar().setValue(
            page.motor_scroll.scroll.verticalScrollBar().maximum())
        self.app.processEvents()
        self.assertEqual(
            page.motor_calibration.mapTo(parent, page.motor_calibration.rect().topLeft()),
            calibration_pos,
        )
        w.resize(1440, 960)

    def test_parameter_constructor(self):
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

        counter = ParameterInput("计数", 1, data_type="uint16", default=0)
        counter.set_value(65535)
        self.assertEqual(counter.value(), 65535)
        with self.assertRaises(ValueError):
            counter.set_value(65536)
        counter.deleteLater()

        flag = ParameterInput("使能", 2, data_type="bool", default=False)
        flag.set_value(True)
        self.assertTrue(flag.value())
        flag.deleteLater()

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

    def test_control_page(self):
        w = self.window
        w.switchTo(w.control_page)
        QTest.qWait(260)
        self.app.processEvents()
        page = w.control_page
        self.assertEqual(len(page.wave_ch), WAVE_CHANNEL_COUNT)
        for combo in page.wave_ch:
            self.assertEqual(combo.itemText(0), "未选择")
            self.assertEqual(combo.count(), len(WAVE_CHANNELS) + 1)
            for index, channel_id in enumerate(WAVE_CHANNELS, 1):
                self.assertEqual(combo.itemText(index), OBSDATA[channel_id].name)
                self.assertEqual(combo.itemData(index), channel_id)
        self.assertEqual(len(page.wave_canvas.channel_colors), WAVE_CHANNEL_COUNT)
        self.assertEqual(len(page.channel_badges), WAVE_CHANNEL_COUNT)
        self.assertEqual(page.channel_card.width(), 192)
        self.assertEqual(page.control_side.width(), 120)
        self.assertEqual(len(page.action_buttons), 3)
        self.assertTrue(all(
            button.parentWidget() is page.control_side for button in page.action_buttons))
        self.assertLess(
            page.control_side.mapTo(
                page.control_card, page.control_side.rect().topLeft()).x(),
            page.target_controls[0].slider.mapTo(
                page.control_card, page.target_controls[0].slider.rect().topLeft()).x(),
        )
        self.assertFalse(hasattr(page, "send_target"))
        for control in page.target_controls:
            self.assertIsInstance(control.send_button, ToolButton)
            self.assertTrue(control.send_button.isEnabled())
            self.assertTrue(control.send_button.toolTip())

        requests = []

        def record_action(action, payload):
            requests.append((action, payload))

        page.action_requested.connect(record_action)
        try:
            page.set_device_enabled(True)
            control = page.target_controls[0]
            control.target.setText("2.5")
            control.target.editingFinished.emit()
            control.send_button.clicked.emit()
            self.assertEqual(requests[-1], (
                "send_target", {"channel": "位置", "value": 2.5}))
        finally:
            page.action_requested.disconnect(record_action)
            page.set_device_enabled(True)

    def test_wave_channel_change_resubscribes(self):
        page = ControlPage()
        requests = []
        page.action_requested.connect(
            lambda action, payload: requests.append((action, payload)))
        try:
            # 未采样时改通道不发任何命令。
            page.wave_ch[0].setCurrentIndex(1)
            self.assertEqual(requests, [])

            page._wave_running = True
            page.add_wave_data(0, 1.0)
            self.assertIsNotNone(page.wave_canvas._channel_view(0))

            requests.clear()
            page.wave_ch[0].setCurrentIndex(2)
            # 采样中改通道：清空旧波形 + 立即按新通道重新订阅。
            self.assertIsNone(page.wave_canvas._channel_view(0))
            self.assertEqual(len(requests), 1)
            action, payload = requests[0]
            self.assertEqual(action, "wave_start")
            self.assertEqual(
                payload["channels"],
                [(0, page.wave_ch[0].currentData())],
            )

            # 取消通道（未选择）同样立即生效。
            requests.clear()
            page.wave_ch[0].setCurrentIndex(0)
            self.assertEqual(requests[0][0], "wave_start")
            self.assertEqual(requests[0][1]["channels"], [])
        finally:
            page.action_requested.disconnect()
            page.deleteLater()

    def test_waveform_draw_reuses_buffer(self):
        from PyQt5.QtCore import QPointF
        canvas = WaveformWidget(max_points=4, min_view_points=2, max_view_points=3)
        try:
            canvas.add_waveform_data(0, [1.0, 2.0, 3.0])
            canvas._batch_update_plot()
            # 刷新只重画有数据的通道，其余通道保持空。
            self.assertEqual(len(canvas.curves[0].getData()[1]), 3)
            self.assertIsNone(canvas.curves[1].getData()[1])

            # 悬停复用绘制缓存，不再重建整段数组。
            center = canvas.plot_widget.sceneBoundingRect().center()
            with patch.object(
                    WaveformWidget, "_channel_view",
                    side_effect=AssertionError("悬停不应重建数组")):
                canvas._on_mouse_hover(QPointF(center))

            # 强制全量刷新与批量刷新走同一条绘制路径。
            with patch.object(canvas, "_redraw") as redraw:
                canvas._batch_update_plot()
                redraw.assert_not_called()  # 没有脏通道就不重画
                canvas.add_waveform_data(1, 5.0)
                canvas._batch_update_plot()
                redraw.assert_called_once_with([1])
        finally:
            canvas.deleteLater()

    def test_waveform_x_window_follows_sample_rate(self):
        canvas = WaveformWidget(max_points=4000)
        try:
            # 固件 1ms 一帧、显示 3s → 3000 点。
            self.assertEqual(canvas.window_points(), 3000)
            # 采样率变化只影响跨度，与波形幅度无关。
            canvas.set_sample_interval(1.25)
            self.assertEqual(canvas.window_points(), 2400)
            self.assertEqual(canvas.window_points(), 2400)
            # 换算结果超过缓冲容量时按容量截断：没有的数据画不出来。
            canvas.set_sample_interval(0.5)
            self.assertEqual(canvas.window_points(), canvas.max_points)
            canvas.set_sample_interval(0.1)
            self.assertEqual(canvas.window_points(), canvas.max_points)
            # 采样间隔未知时退回 max_view_points。
            canvas.set_sample_interval(None)
            self.assertEqual(canvas.window_points(), canvas.max_view_points)

            canvas.set_sample_interval(1.0)
            for value in range(3500):
                canvas.add_waveform_data(0, float(value % 100))
            canvas.auto_scale_x_axis()
            x_range = canvas.plot_widget.getViewBox().viewRange()[0]
            self.assertAlmostEqual(x_range[1], 3500, places=3)
            self.assertAlmostEqual(x_range[1] - x_range[0], 3000, places=3)

            # 数据不足一个窗口时左对齐到 0。
            canvas.clear_waveforms()
            canvas.add_waveform_data(0, 1.0)
            canvas.auto_scale_x_axis()
            x_range = canvas.plot_widget.getViewBox().viewRange()[0]
            self.assertAlmostEqual(x_range[0], 0, places=3)
            self.assertAlmostEqual(x_range[1], 3000, places=3)
        finally:
            canvas.deleteLater()

    def test_waveform_ring_buffer(self):
        canvas = WaveformWidget(max_points=4, min_view_points=2, max_view_points=3)
        try:
            for value in (1.0, 2.0, 3.0, 4.0):
                canvas.add_waveform_data(0, value)
            self.assertEqual(
                canvas._channel_view(0).tolist(), [1.0, 2.0, 3.0, 4.0])
            # 缓冲满后环形覆盖最旧数据，顺序仍是旧 → 新。
            for value in (5.0, 6.0):
                canvas.add_waveform_data(0, value)
            self.assertEqual(
                canvas._channel_view(0).tolist(), [3.0, 4.0, 5.0, 6.0])
            # 同一采样值可同时写入多行（多行订阅同一信号）。
            canvas.add_waveform_data((1, 2), 7.0)
            self.assertEqual(canvas._channel_view(1).tolist(), [7.0])
            self.assertEqual(canvas._channel_view(2).tolist(), [7.0])
            self.assertIsNone(canvas._channel_view(3))
            canvas.clear_waveforms()
            self.assertIsNone(canvas._channel_view(0))
        finally:
            canvas.deleteLater()

    def test_stream_frames_skip_terminal(self):
        service = self.window.device_service
        service.terminal.clear_log()
        with patch.object(service, "_stream_channels", []):
            service._on_frame(
                CmdId.CMD_DATA_STREAM,
                codec.pack_data_stream([Didx.DATA_POSITION], [1.0]))
            service._on_frame(
                CmdId.CMD_HEARTBEAT,
                codec.pack_heartbeat([1, 0, 48.0, 40.0, 41.0]))
        self.assertEqual(service.terminal.output.toPlainText().strip(), "")
        # 事件性回包仍然要留痕。
        service._on_frame(CmdId.CMD_CONNECT, b"")
        self.assertIn("CMD_CONNECT", service.terminal.output.toPlainText())

    def test_data_stream_subscription_mapping(self):
        page = ControlPage()
        service = self.window.device_service
        try:
            with patch.object(service, "control_page", page), \
                    patch.object(service, "_connected", False):
                service._wave_map = {
                    0: Didx.DATA_POSITION,
                    1: Didx.DATA_POSITION,
                    2: Didx.DATA_VELOCITY,
                }
                service._set_subscription()
                # 同一信号只订阅一次，但两行都要上屏。
                self.assertEqual(
                    service._stream_channels,
                    [Didx.DATA_POSITION, Didx.DATA_VELOCITY])
                self.assertEqual(
                    service._rows_by_channel[Didx.DATA_POSITION], [0, 1])

                payload = codec.pack_data_stream(
                    service._stream_channels, [1.5, -2.5])
                service._handle_data_stream(payload)
                self.assertEqual(
                    page.wave_canvas._channel_view(0).tolist(), [1.5])
                self.assertEqual(
                    page.wave_canvas._channel_view(1).tolist(), [1.5])
                self.assertEqual(
                    page.wave_canvas._channel_view(2).tolist(), [-2.5])

                # 停止采样后到达的残留帧没有解码上下文，不能抛异常。
                service._wave_map = {}
                service._set_subscription()
                self.assertEqual(service._stream_channels, [])
                service._handle_data_stream(payload)
        finally:
            page.deleteLater()

    def test_slider_changes_send_targets(self):
        page = ControlPage()
        service = self.window.device_service
        page.action_requested.connect(service._handle_control_action)
        cases = [
            (eMotorState.MOTOR_OPENLOOP, page.openloop_duty, None,
             CmdId.CMD_OPENLOOP_CTRL, lambda: codec.pack_openloop_ctrl(50, 0)),
            (eMotorState.MOTOR_OPENLOOP, page.openloop_control, None,
             CmdId.CMD_OPENLOOP_CTRL, lambda: codec.pack_openloop_ctrl(50, 3.14)),
            (eMotorState.MOTOR_OPENLOOP, page.openloop_control,
             lambda: page.openloop_kind.setCurrentIndex(1),
             CmdId.CMD_OPENLOOP_CTRL, lambda: codec.pack_openloop_ctrl(50, 100)),
            (eMotorState.MOTOR_CURRENT_MODE, page.current_torque_control, None,
             CmdId.CMD_CUR_TAU_CTRL, lambda: codec.pack_current_torque(5)),
            (eMotorState.MOTOR_CURRENT_MODE, page.current_torque_control,
             lambda: page.current_torque_kind.setCurrentIndex(1),
             CmdId.CMD_CUR_TAU_CTRL, lambda: codec.pack_current_torque(2.5)),
            (eMotorState.MOTOR_PID_SPEED, page.speed_control, None,
             CmdId.CMD_PID_VEL_CTRL, lambda: codec.pack_single(50)),
            (eMotorState.MOTOR_PID_POSITION, page.position_control, None,
             CmdId.CMD_PID_POS_CTRL, lambda: codec.pack_single(90)),
            (eMotorState.MOTOR_MIT_MODE, page.mit_position, None,
             CmdId.CMD_MIT_CTRL, lambda: codec.pack_mit_ctrl(90)),
        ]
        try:
            with patch.object(service, "control_page", page), \
                    patch.object(service, "_connected", True), \
                    patch.object(service.serial, "send", return_value=True) as send:
                for mode, control, setup, command, expected in cases:
                    with self.subTest(mode=mode, channel=control.name, setup=setup):
                        page.set_mode(mode)
                        if setup:
                            setup()
                        send.reset_mock()
                        control.slider.setValue(5000)
                        send.assert_called_once_with(command, expected())
                        # 同值不重复发送；手动发送按钮仍然可用。
                        control.slider.setValue(5000)
                        send.assert_called_once_with(command, expected())
                        control.send_button.clicked.emit()
                        self.assertEqual(send.call_count, 2)

                send.reset_mock()
                service._connected = False
                page.mit_position.slider.setValue(2500)
                send.assert_not_called()
        finally:
            page.deleteLater()

    def test_target_updates_do_not_send(self):
        page = ControlPage()
        requests = []
        page.action_requested.connect(lambda action, payload: requests.append((action, payload)))
        try:
            control = page.openloop_control
            control.slider.setValue(5000)
            requests.clear()
            page.openloop_kind.setCurrentIndex(1)
            control.maximum.setText("0")
            control.maximum.editingFinished.emit()
            control.maximum.setText("-10")
            control.maximum.editingFinished.emit()
            page.set_targets({"位置": 2.5, "速度": 3, "电流": 1})
            self.assertEqual(requests, [])
            control.slider.setValue(5000)
            self.assertEqual(requests, [
                ("send_target", {"channel": "开环速度", "value": -5.0}),
            ])
            requests.clear()
            # 非零滑条改成零跨度，也不应下发控制命令。
            control.maximum.setText("0")
            control.maximum.editingFinished.emit()
            self.assertEqual(requests, [])
        finally:
            page.deleteLater()

    def test_ctrl_config_core_reset_and_mit_branch(self):
        service = self.window.device_service
        page = self.window.control_page
        mode_control = {
            eMotorState.MOTOR_OPENLOOP: lambda: page.openloop_kind.setCurrentIndex(1),
            eMotorState.MOTOR_CURRENT_MODE:
                lambda: page.current_torque_kind.setCurrentIndex(1),
            eMotorState.MOTOR_PID_SPEED: lambda: None,
            eMotorState.MOTOR_PID_POSITION: lambda: None,
            eMotorState.MOTOR_MIT_MODE: lambda: None,
        }
        try:
            with patch.object(service, "_connected", True), \
                    patch.object(service.serial, "send", return_value=True) as send:
                for mode, control in (
                    (eMotorState.MOTOR_OPENLOOP, 1),
                    (eMotorState.MOTOR_CURRENT_MODE, 1),
                    (eMotorState.MOTOR_PID_SPEED, 0),
                    (eMotorState.MOTOR_PID_POSITION, 0),
                ):
                    with self.subTest(mode=mode):
                        page.set_mode(mode)
                        mode_control[mode]()
                        send.reset_mock()
                        service._handle_control_action("enable", None)
                        # 使能只发一次配置，不再补发目标值。
                        send.assert_called_once_with(
                            CmdId.CMD_CTRL_CONFIG,
                            codec.pack_ctrl_config(mode, control),
                        )

                page.set_mode(eMotorState.MOTOR_MIT_MODE)
                send.reset_mock()
                service._handle_control_action(
                    "send_mit_config", {"csv": "", "period_ms": 100})
                self.assertEqual(send.call_count, 1)
                self.assertEqual(send.call_args_list[0][0][0], CmdId.CMD_CTRL_CONFIG)
                payload = send.call_args_list[0][0][1]
                self.assertEqual(len(payload), 8)
                self.assertEqual(payload[0], eMotorState.MOTOR_MIT_MODE)

                send.reset_mock()
                service._handle_status_action("core_reset")
                send.assert_called_once_with(CmdId.CMD_CORE_NRST, b"")
        finally:
            page.openloop_kind.setCurrentIndex(0)
            page.current_torque_kind.setCurrentIndex(0)
            page.set_mode(eMotorState.MOTOR_OPENLOOP)

    def test_status_bar_reset_buttons_emit_actions(self):
        status = self.window.top_area
        actions = []
        status.action_requested.connect(actions.append)
        try:
            status.reset_button.clicked.emit()
            status.system_reset_button.clicked.emit()
            self.assertEqual(actions, ["core_reset", "system_reset"])
        finally:
            status.action_requested.disconnect(actions.append)

    def test_target_ranges(self):
        page = self.window.control_page
        for control in page.target_controls:
            self.assertEqual(control.minimum.text(), "0")
            self.assertEqual(control.minimum.accessibleName(),
                             control.target.accessibleName().replace("目标值", "最小值"))
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

    def test_control_modes(self):
        page = self.window.control_page
        # 其他测试可能切换过模式，先回到开环页再验证。
        page.set_mode(eMotorState.MOTOR_OPENLOOP)
        self.assertIsInstance(page.mode_nav, SegmentedWidget)
        self.assertEqual(page.mode_stack.count(), len(MODE_PAGE_MODES))
        self.assertEqual(len(page.mode_routes), len(MODE_PAGE_MODES))
        self.assertEqual(len(page.enable_buttons), len(MODE_PAGE_MODES))
        self.assertTrue(all(button.isEnabled() for button in page.enable_buttons))
        self.assertEqual(page.get_mode(), eMotorState.MOTOR_OPENLOOP)
        self.assertEqual(page.mode_stack.currentIndex(), 0)
        self.assertFalse(page.mode_nav.isHidden())
        self.assertEqual(page.mode_nav.currentRouteKey(), page.mode_routes[0])

        page.set_mode(eMotorState.MOTOR_OPENLOOP)
        self.assertEqual(page.mode_stack.currentIndex(), 0)
        self.assertEqual(page.mode_nav.currentRouteKey(), page.mode_routes[0])
        self.assertEqual(page.openloop_kind.count(), 2)
        self.assertEqual(page.openloop_control.name, "开环角度")
        self.assertEqual(page.openloop_control.label.text(), "开环角度(rad)")
        page.openloop_kind.setCurrentIndex(1)
        self.assertEqual(page.openloop_control.name, "开环速度")
        self.assertEqual(page.openloop_control.label.text(), "开环速度(rad/s)")
        self.assertEqual(page.openloop_control.maximum.text(), "200")

        page.set_mode(eMotorState.MOTOR_CURRENT_MODE)
        self.assertEqual(page.mode_stack.currentIndex(), 1)
        self.assertEqual(page.current_torque_kind.count(), 2)
        self.assertEqual(page.current_torque_control.name, "电流")
        self.assertEqual(page.current_torque_control.label.text(), "电流(A)")
        page.current_torque_kind.setCurrentIndex(1)
        self.assertEqual(page.current_torque_control.name, "力矩")
        self.assertEqual(page.current_torque_control.label.text(), "力矩(N·m)")

        page.set_mode(eMotorState.MOTOR_PID_SPEED)
        self.assertEqual(page.mode_stack.currentIndex(), 2)
        page.set_mode(eMotorState.MOTOR_PID_POSITION)
        self.assertEqual(page.mode_stack.currentIndex(), 3)

        page.set_mode(eMotorState.MOTOR_MIT_MODE)
        self.assertEqual(page.mode_stack.currentIndex(), 4)
        self.assertEqual(page.mit_period.value(), 100)
        self.assertEqual(page.mit_kind.count(), 2)
        self.assertEqual(page.mit_stack.count(), 2)
        self.assertIsInstance(page.mit_csv_button, ToolButton)
        self.assertTrue(page.mit_config_send.property("deviceAction"))
        self.assertTrue(page.mit_config_send.isEnabled())
        self.assertEqual(page.mit_config_send.text(), "开始执行")

        # 导航只切页，不发送模式命令。
        page.set_mode(eMotorState.MOTOR_OPENLOOP)
        page.mode_nav.setCurrentItem(page.mode_routes[1])
        self.assertEqual(page.mode_stack.currentIndex(), 1)
        self.assertEqual(page.get_mode(), eMotorState.MOTOR_CURRENT_MODE)

        requests = []

        def record_action(action, payload):
            requests.append((action, payload))

        page.action_requested.connect(record_action)
        try:
            page.set_mode(eMotorState.MOTOR_OPENLOOP)
            page.mode_nav.setCurrentItem(page.mode_routes[1])
            self.assertFalse(any(action == "set_mode" for action, _ in requests))

            page.enable_buttons[0].clicked.emit()
            self.assertEqual(requests[-1][0], "enable")

            page.set_mode(eMotorState.MOTOR_MIT_MODE)
            page.set_device_enabled(True)
            page.mit_kind.setCurrentIndex(1)
            page.mit_csv_path.setText("/tmp/mit.csv")
            page.mit_period.setValue(200)
            page.mit_config_send.clicked.emit()
            self.assertEqual(requests[-1][0], "send_mit_config")
            self.assertEqual(requests[-1][1]["csv"], "/tmp/mit.csv")
            self.assertEqual(requests[-1][1]["period_ms"], 200)
        finally:
            page.action_requested.disconnect(record_action)
            page.set_device_enabled(True)
            page.openloop_kind.setCurrentIndex(0)
            page.current_torque_kind.setCurrentIndex(0)
            page.mit_kind.setCurrentIndex(0)
            page.set_mode(eMotorState.MOTOR_OPENLOOP)

    def test_parameter_defaults_from_protocol(self):
        page = self.window.config_page
        # 其他测试可能已经改过参数，这里重新加载默认配置后再核对协议默认值。
        self.window.device_service._load_config("default", announce=False)
        for parameter in page.parameters.values():
            meta = PARAMETERS.get(parameter.protocol_id)
            if meta is not None:
                self.assertEqual(parameter.value(), meta.default)

    def test_fluent_text_dialog(self):
        from app.dialogs import TextInputDialog
        from qfluentwidgets.components.dialog_box import MessageBoxBase

        dialog = TextInputDialog("保存配置", "配置名称：", "default", self.window)
        try:
            self.assertIsInstance(dialog, MessageBoxBase)
            self.assertEqual(dialog.value(), "default")
        finally:
            dialog.close()

    def test_firmware_page(self):
        page = self.window.download_page
        self.assertEqual(set(page.device_card.parameters), {
            "设备型号", "Bootloader 版本", "Application 版本",
        })
        for parameter in page.device_card.parameters.values():
            self.assertTrue(parameter.read_only)
            self.assertFalse(parameter.show_actions)
            for button in (parameter.write_button, parameter.default_button, parameter.read_button):
                self.assertTrue(button.isHidden())

        self.assertIsInstance(page.driver_output, PlainTextEdit)
        page.set_driver_log("line1\nline2")
        self.assertEqual(page.driver_output.toPlainText(), "line1\nline2")
        page.append_driver_log("line3")
        self.assertIn("line3", page.driver_output.toPlainText())
        page.clear_driver_log()
        self.assertEqual(page.driver_output.toPlainText(), "")

        self.assertEqual(len(page.firmware_panels), 2)
        for route, select_button in page.firmware_select_buttons.items():
            self.assertIsInstance(select_button, ToolButton)
            self.assertFalse(select_button.property("deviceAction"))
            card = page.firmware_panels[route]
            self.assertEqual(len(card.findChildren(PushButton)), 2)
            self.assertEqual(len(card.findChildren(LineEdit)), 1)
        page.set_progress("bootloader", 42)
        self.assertEqual(
            page.firmware_panels["bootloader"].findChildren(ProgressBar)[0].value(), 42)

    def test_page_and_status_api(self):
        w = self.window
        status = w.top_area
        status.update_status({"voltage": "48.0 V", "state": "运行"})
        self.assertEqual(status.indicators["voltage"].text(), "48.0 V")
        self.assertEqual(status.indicators["state"].text(), "运行")

        w.config_page.set_parameter(Pidx.PARAM_MOTOR_RS, 1.25)
        self.assertEqual(w.config_page.get_parameter(Pidx.PARAM_MOTOR_RS), 1.25)
        self.assertIn(Pidx.PARAM_MOTOR_RS, w.config_page.parameters)
        w.control_page.set_targets({"位置": "2.5"})
        self.assertEqual(w.control_page.targets[0].text(), "2.50")
        w.terminal_page.set_log("test")
        self.assertEqual(w.terminal_page.output.toPlainText(), "test")

    def test_business_layer(self):
        w = self.window
        service = w.device_service
        self.assertIn("default", service.config_store.names())
        stored = service.config_store.load("default")
        service._load_config("default", announce=False)
        if "PARAM_MOTOR_RS" in stored:
            self.assertAlmostEqual(
                w.config_page.get_parameter(Pidx.PARAM_MOTOR_RS),
                float(stored["PARAM_MOTOR_RS"]),
                places=9,
            )
        values = service._collect_config_values()
        self.assertIn("PARAM_MOTOR_RS", values)

        self.assertEqual(
            service._build_target_payload("位置", 0.0)[0], CmdId.CMD_PID_POS_CTRL)
        w.control_page.set_mode(eMotorState.MOTOR_MIT_MODE)
        self.assertEqual(
            service._build_target_payload("位置", 0.0)[0], CmdId.CMD_MIT_CTRL)
        w.control_page.set_mode(eMotorState.MOTOR_IDLE)

        packet = service.serial._build_packet(CmdId.CMD_CONNECT, b"")
        self.assertEqual(packet[0], UartFrame.UART_PACKET_HEAD)
        self.assertEqual(packet[-1], UartFrame.UART_PACKET_TAIL)
        self.assertEqual(packet[1], CmdId.CMD_CONNECT)
        self.assertEqual(len(packet), 5)
        received = []
        service.serial.frame_received.connect(
            lambda cmd_id, data: received.append((cmd_id, data)))
        service.serial._buffer = bytearray(packet)
        service.serial._parse_buffer()
        self.assertEqual(received, [(CmdId.CMD_CONNECT, b"")])

    def test_payload_codec(self):
        rs = PARAMETERS[Pidx.PARAM_MOTOR_RS]
        payload = codec.pack_param(rs, 1.25)
        meta, value = codec.unpack_param(payload)
        self.assertEqual(meta.id, rs.id)
        self.assertAlmostEqual(value, 1.25)

        records = [
            (PARAMETERS[Pidx.PARAM_MOTOR_TYPE], 1),
            (PARAMETERS[Pidx.PARAM_MOTOR_POLEPAIRS], 7),
        ]
        batch = codec.pack_params_all_write(records)
        start, restored = codec.unpack_params_all_read(batch)
        self.assertEqual(start, Pidx.PARAM_MOTOR_TYPE)
        self.assertEqual([(m.id, v) for m, v in restored],
                         [(m.id, v) for m, v in records])

        status_values = [1, 0, 48.0, 40.0, 41.0]
        heartbeat = codec.pack_heartbeat(status_values)
        self.assertEqual(codec.unpack_heartbeat(heartbeat), status_values)

        wave_ids = [Didx.DATA_POSITION]
        data_stream = codec.pack_data_stream(wave_ids, [3.14])
        got_wave = codec.unpack_data_stream(data_stream, wave_ids)
        self.assertAlmostEqual(got_wave[0], 3.14, places=5)
        with self.assertRaises(ValueError):
            codec.pack_stream_set(list(range(6)))

        self.assertEqual(len(codec.pack_openloop_ctrl(0.5, 1.0)), 6)
        self.assertEqual(len(codec.pack_current_torque(2.0)), 4)
        self.assertEqual(len(codec.pack_single(1.0)), 4)
        self.assertEqual(len(codec.pack_mit_ctrl(1, 2, 3)), 8)
        self.assertEqual(len(codec.pack_ctrl_config(4, 1)), 2)
        self.assertEqual(len(codec.pack_ctrl_config_mit(1, 2)), 8)
        self.assertEqual(codec.pack_connect(True), b"\x01")
        self.assertEqual(len(codec.pack_iap_erase(1024)), 4)
        self.assertEqual(len(codec.pack_iap_write(0, b"x" * 120)), 124)
        ack = struct.pack("<IB", 120, FbId.FB_EXECUTE)
        offset, status = codec.unpack_iap_write_ack(ack)
        self.assertEqual(offset, 120)
        self.assertEqual(status, FbId.FB_EXECUTE)

    def test_output_and_business_isolation(self):
        w = self.window
        for output in (w.top_area.host_output, w.terminal_page.output):
            self.assertTrue(output.isReadOnly())
        self.assertFalse(any(name in sys.modules for name in [
            "functions.com_port", "functions.wave", "functions.IAP_downloader",
        ]))
        for button in w.findChildren(PushButton):
            if button.property("deviceAction"):
                self.assertTrue(button.isEnabled())
        self.assertEqual(set(w.download_page.device_card.fields), {
            "设备型号", "Bootloader 版本", "Application 版本",
        })
        self.assertEqual(len(w.download_page.firmware_panels), 2)
        self.assertEqual(len(w.monitor_page.tabs), 2)

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
                if isinstance(editor, PlainTextEdit):
                    self.assertTrue(editor.tabChangesFocus())
            for card in w.findChildren(SectionCard):
                for field in card.fields.values():
                    self.assertEqual(field.height(), 32)

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
            config = w.config_page
            self.assertLess(config.stack.mapTo(w, config.stack.rect().topRight()).x(),
                            config.action_panel.mapTo(w, config.action_panel.rect().topLeft()).x())
        w.resize(1440, 960)


if __name__ == "__main__":
    unittest.main()
