"""设备业务层：把新 UI 的信号接到串口、配置和命令 ID。

说明：协议 payload 的具体字段/字节序尚未确认，所有需要 payload 的命令
统一走 `_send_todo()`，只记录命令 ID 和操作，后续在协议确定后补编解码。
"""

from pathlib import Path
import struct

from PyQt5.QtWidgets import QDialog, QFileDialog
from PyQt5.QtCore import QObject
from qfluentwidgets import FluentIcon as FIF, MessageBox

from protocol import (
    CmdId, FbId, PARAMETERS, PARAM_BY_NAME, Pidx, Didx, OBSDATA, eCoreState,
)
from app.dialogs import TextInputDialog
from app.services.config_store import ConfigStore
from app.services.serial_service import SerialService
from app.services.iap_worker import IapWorker
from app.services import payload_codec as codec


CONFIG_DIR = Path(__file__).resolve().parents[2] / "config"
STATUS_CHANNELS = (
    Didx.DATA_STATE,
    Didx.DATA_FAULT,
    Didx.DATA_WARNING,
    Didx.DATA_VBUS,
    Didx.DATA_DRIVER_TEMP,
    Didx.DATA_MOTOR_TEMP,
)
STREAM_MAX_CHANNELS = 5
struct_error = struct.error


class DeviceService(QObject):
    def __init__(self, window):
        super().__init__(window)
        self.window = window
        self.status = window.top_area
        self.terminal = window.terminal_page
        self.config_page = window.config_page
        self.control_page = window.control_page
        self.iap_page = window.download_page
        self.pages = (self.config_page, self.control_page, self.iap_page)

        self.serial = SerialService(self)
        self.config_store = ConfigStore(CONFIG_DIR)
        self._connected = False
        self._firmware_files = {}
        self._cmd_names = self._build_cmd_names()
        self._wave_map = {}
        self._stream_channels = []
        self._iap_worker = None
        self._iap_route = None

        self._wire()
        self._refresh_configs(initial=True)
        # UI 操作始终可用；未连接时业务层记录并忽略，避免按钮全灰。
        for page in self.pages:
            page.set_device_enabled(True)
        self.status.system_reset_button.setEnabled(True)
        self.status.reset_button.setEnabled(True)
        self._host("上位机就绪；payload 使用小端二进制，示波订阅最多 5 路。")

    @staticmethod
    def _build_cmd_names():
        names = {}
        for name in dir(CmdId):
            if name.startswith("_"):
                continue
            value = getattr(CmdId, name)
            if isinstance(value, int):
                names.setdefault(value, name)
        return names

    # ---------- 信号连接 ----------
    def _wire(self):
        self.status.connect_but.clicked.connect(self._toggle_connection)
        self.status.action_requested.connect(self._handle_status_action)
        self.status.com_port.currentIndexChanged.connect(self._sync_connect_button)

        self.serial.ports_changed.connect(self._on_ports_changed)
        self.serial.connected.connect(self._on_connected)
        self.serial.disconnected.connect(self._on_disconnected)
        self.serial.frame_received.connect(self._on_frame)
        self.serial.log.connect(self._host)
        self.serial.error.connect(self._host)

        for page in self.pages:
            page.parameter_read_requested.connect(self._read_parameter)
            page.parameter_write_requested.connect(self._write_parameter)
            page.parameter_default_requested.connect(self._default_parameter)
            page.parameter_changed.connect(self._parameter_changed)
            page.validation_failed.connect(self._validation_failed)
            page.action_requested.connect(
                lambda action, payload, p=page: self._handle_action(p, action, payload))

    # ---------- 连接 ----------
    def _on_ports_changed(self, entries):
        combo = self.status.com_port
        current = combo.currentData()
        current_device = current.get("device") if isinstance(current, dict) else None
        combo.blockSignals(True)
        combo.clear()
        if entries:
            for entry in entries:
                combo.addItem(entry["display"], userData=entry)
            if current_device:
                for index in range(combo.count()):
                    data = combo.itemData(index)
                    if isinstance(data, dict) and data.get("device") == current_device:
                        combo.setCurrentIndex(index)
                        break
        else:
            combo.addItem("未选择端口", userData=None)
        combo.blockSignals(False)
        self._sync_connect_button()

    def _sync_connect_button(self):
        if self._connected:
            self.status.connect_but.setEnabled(True)
            return
        data = self.status.com_port.currentData()
        self.status.connect_but.setEnabled(isinstance(data, dict))

    def _toggle_connection(self):
        if self._connected:
            self.serial.disconnect("手动断开")
            return
        entry = self.status.com_port.currentData()
        if not isinstance(entry, dict):
            self._host("请选择可用串口")
            return
        if not self.serial.connect_to(entry):
            self._host("连接失败，请检查串口权限或设备状态")

    def _on_connected(self, entry):
        self._connected = True
        self.status.connect_but.setIcon(FIF.CLOSE)
        self.status.connect_but.setToolTip("断开设备")
        self.status.connect_but.setAccessibleName("断开设备")
        self.status.com_port.setEnabled(False)
        self.status.connect_but.setEnabled(True)
        self.status.system_reset_button.setEnabled(True)
        self.status.reset_button.setEnabled(True)
        for page in self.pages:
            page.set_device_enabled(True)
        self._set_subscription()
        self._host(f"已连接：{entry.get('display', entry.get('device'))}")
        self.iap_page.append_driver_log(f"已连接：{entry.get('display', '')}")

    def _on_disconnected(self, reason):
        self._connected = False
        self._wave_map = {}
        self._stream_channels = []
        if self._iap_worker is not None:
            self._iap_worker.stop()
        self.status.connect_but.setIcon(FIF.LINK)
        self.status.connect_but.setToolTip("连接设备")
        self.status.connect_but.setAccessibleName("连接设备")
        self.status.com_port.setEnabled(True)
        self.status.system_reset_button.setEnabled(True)
        self.status.reset_button.setEnabled(True)
        for page in self.pages:
            page.set_device_enabled(True)
        self._sync_connect_button()
        self._host(f"已断开：{reason}")
        self.iap_page.append_driver_log(f"已断开：{reason}")

    # ---------- 接收 ----------
    def _on_frame(self, cmd_id, payload):
        name = self._cmd_names.get(cmd_id, f"0x{cmd_id:02X}")
        self._terminal(f"RX {name} ({cmd_id:#04x}) len={len(payload)} {payload.hex(' ')}")
        if self._iap_worker is not None:
            self._iap_worker.push_frame(cmd_id, payload)
        try:
            if cmd_id == CmdId.CMD_HEARTBEAT:
                self._handle_stream(payload)
                return

            if cmd_id == CmdId.CMD_PARAM_READ:
                if len(payload) >= 2:
                    meta, value = codec.unpack_param(payload)
                    self.config_page.set_protocol_parameter(meta.id, value)
                    self._host(f"读取 {meta.name} = {value}")
                return

            if cmd_id == CmdId.CMD_PARAM_ALLREAD:
                start, records = codec.unpack_params_all_read(payload)
                for meta, value in records:
                    self.config_page.set_protocol_parameter(meta.id, value)
                if not records:
                    self._host(f"参数批量读取完成（起点 {start}）")
                return

            if cmd_id in (
                CmdId.CMD_PARAM_WRITE, CmdId.CMD_PARAM_ALLWRITE,
                CmdId.CMD_PARAM_SAVE, CmdId.CMD_PARAM_RELOAD,
                CmdId.CMD_PARAM_ALLRELOAD, CmdId.CMD_DATA_STREAM_SET,
            ):
                if payload:
                    self._host(
                        f"{name}：{'成功' if payload[0] == FbId.FB_EXECUTE else '失败'}")
                return

            if cmd_id == CmdId.CMD_IAP_WRITE_FLASH:
                offset, status = codec.unpack_iap_write_ack(payload)
                self._host(
                    f"IAP 写入 offset={offset}："
                    f"{'成功' if status == FbId.FB_EXECUTE else '失败'}")
                return

            if cmd_id in (
                CmdId.CMD_IAP_ERASE_FLASH, CmdId.CMD_IAP_VERIFY_FLASH,
                CmdId.CMD_IAP_EXIT,
            ):
                if payload:
                    self._host(
                        f"{name}：{'成功' if payload[0] == FbId.FB_EXECUTE else '失败'}")
                return

            if cmd_id in (CmdId.CMD_ASK_APP_VERSION, CmdId.CMD_ASK_BL_VERSION,
                          CmdId.CMD_ASK_DEVICE_MESSAGE):
                text = self._decode_text(payload)
                if text:
                    self.iap_page.append_driver_log(f"{name}: {text}")
                    if cmd_id == CmdId.CMD_ASK_APP_VERSION:
                        self.iap_page.set_device_info({"Application 版本": text})
                    elif cmd_id == CmdId.CMD_ASK_BL_VERSION:
                        self.iap_page.set_device_info({"Bootloader 版本": text})
                    elif cmd_id == CmdId.CMD_ASK_DEVICE_MESSAGE:
                        self.iap_page.set_device_info({"设备型号": text})
                return

            if cmd_id == CmdId.CMD_CONNECT:
                return

            if payload and payload[0] in (FbId.FB_EXECUTE, FbId.FB_FAILURE):
                result = "成功" if payload[0] == FbId.FB_EXECUTE else "失败"
                self._host(f"{name}：{result}")
                return

            self._terminal(f"未处理回包：{name}")
        except (TypeError, ValueError, struct_error) as error:
            self._host(f"解析 {name} 失败：{error}")

    @staticmethod
    def _decode_text(payload):
        return payload.rstrip(b"\x00\xff").decode("utf-8", errors="ignore").strip()

    def _handle_stream(self, payload):
        status_values, values = codec.unpack_heartbeat(
            payload, self._stream_channels)
        for channel_id, value in zip(STATUS_CHANNELS, status_values):
            self._update_observed(channel_id, value)
        for row, channel_id in self._wave_map.items():
            if channel_id not in self._stream_channels:
                continue
            index = self._stream_channels.index(channel_id)
            if index < len(values):
                self.control_page.add_wave_data(row, values[index])

    def _update_observed(self, channel_id, value):
        if channel_id == Didx.DATA_STATE:
            self.status.update_status({"state": self._option_text(channel_id, value)})
        elif channel_id == Didx.DATA_FAULT:
            self.status.update_status({
                "error": "—" if value == 0 else self._option_text(channel_id, value)})
        elif channel_id == Didx.DATA_WARNING:
            if value != 0:
                self.status.update_status({"error": self._option_text(channel_id, value)})
        elif channel_id == Didx.DATA_VBUS:
            self.status.update_status({"voltage": f"{float(value):.2f} V"})
        elif channel_id == Didx.DATA_DRIVER_TEMP:
            self.status.update_status({"temperature": f"{float(value):.1f} °C"})
        elif channel_id == Didx.DATA_MOTOR_TEMP:
            self.status.update_status({"motor_temperature": f"{float(value):.1f} °C"})

    @staticmethod
    def _option_text(channel_id, value):
        meta = OBSDATA.get(channel_id)
        if meta and meta.options:
            for text, option_value in meta.options:
                if option_value == value:
                    return text
        return str(value)

    # ---------- 参数 ----------
    def _read_parameter(self, protocol_id):
        meta = PARAMETERS.get(protocol_id)
        if not meta:
            return
        self._send_payload(
            CmdId.CMD_PARAM_READ, bytes([meta.id]), f"读取参数 {meta.name}")

    def _write_parameter(self, protocol_id, value):
        meta = PARAMETERS.get(protocol_id)
        if not meta:
            return
        try:
            payload = codec.pack_param(meta, value)
        except (TypeError, ValueError, struct_error) as error:
            self._host(f"{meta.name} 编码失败：{error}")
            return
        self._send_payload(
            CmdId.CMD_PARAM_WRITE, payload, f"写入参数 {meta.name} = {value}")

    def _default_parameter(self, protocol_id):
        meta = PARAMETERS.get(protocol_id)
        if not meta:
            return
        self.config_page.set_protocol_parameter(protocol_id, meta.default)
        self._host(f"{meta.name} 已恢复协议默认值：{meta.default}")

    def _parameter_changed(self, protocol_id, value):
        # UI 本地变更不立即下发，由写入按钮/全部写入触发。
        pass

    def _validation_failed(self, protocol_id, error):
        self._host(f"参数 {protocol_id} 校验失败：{error}")

    # ---------- 页面动作 ----------
    def _handle_action(self, page, action, payload):
        payload = payload or {}
        if page is self.config_page:
            self._handle_config_action(action, payload)
        elif page is self.control_page:
            self._handle_control_action(action, payload)
        elif page is self.iap_page:
            self._handle_iap_action(action, payload)

    def _handle_config_action(self, action, payload):
        if action == "load_config":
            self._load_config(self.config_page.config_file.currentText())
        elif action == "save_config_as":
            self._save_config_as()
        elif action == "delete_config":
            self._delete_config()
        elif action == "read_parameters":
            self._send_payload(CmdId.CMD_PARAM_ALLREAD, b"", "读取全部参数")
        elif action == "write_parameters":
            self._write_all_parameters()
        elif action == "save_to_device":
            self._send_empty(CmdId.CMD_PARAM_SAVE, "保存参数")
        elif action == "restore_defaults":
            self._send_empty(CmdId.CMD_PARAM_ALLRELOAD, "恢复默认参数")
            self._load_config("default", announce=True)

    def _handle_control_action(self, action, payload):
        if action == "set_mode":
            self._host(
                f"控制模式 -> {payload.get('label')} ({payload.get('mode')})")
        elif action == "send_target":
            channel = payload.get("channel")
            command, data = self._build_target_payload(channel, payload.get("value", 0.0))
            if command is None:
                self._host(f"未知目标通道：{channel}")
            else:
                self._send_payload(
                    command, data, f"发送目标 {channel} = {payload.get('value')}")
        elif action == "send_mit_config":
            self._send_todo(
                CmdId.CMD_MIT_CONFIG,
                f"MIT CSV 轨迹播放 csv={payload.get('csv')} period={payload.get('period_ms')}ms",
            )
        elif action == "wave_start":
            channels = payload.get("channels", [])
            self._wave_map = {
                int(row): int(channel_id) for row, channel_id in channels
            }
            self._set_subscription()
        elif action == "wave_stop":
            self._wave_map = {}
            self._set_subscription()
        elif action == "disable":
            self._send_empty(CmdId.CMD_DISABLE, "释放电机")
        elif action == "set_zero":
            self._send_empty(CmdId.CMD_SET_ZERO_POS, "设置零点")
        elif action == "set_limit":
            self._send_empty(CmdId.CMD_SET_LIMIT_POS, "以当前位置设置极限位置")
        elif action == "calibrate":
            for protocol_id, value in (payload.get("params") or {}).items():
                self._write_parameter(int(protocol_id), value)
            command = payload.get("command")
            if command is not None:
                self._send_empty(
                    command,
                    f"校准 {payload.get('scope')} / {payload.get('operation')}",
                )
        elif action == "calibration_invalid":
            self._host(f"校准参数无效：{payload.get('error')}")

    def _build_target_payload(self, channel, value):
        mode = self.control_page.get_mode()
        value = float(value)
        if channel == "位置":
            if mode == eCoreState.CORE_MIT_MODE:
                return CmdId.CMD_MIT_POS, codec.pack_mit_full(
                    self._parameter_value(Pidx.PARAM_MIT_KP, 0.0),
                    self._parameter_value(Pidx.PARAM_MIT_KD, 0.0),
                    value,
                    0.0,
                    0.0,
                )
            return CmdId.CMD_PID_POS, codec.pack_single(value)
        if channel == "速度":
            return CmdId.CMD_PID_VEL, codec.pack_single(value)
        if channel in ("电流", "力矩"):
            is_torque = (
                channel == "力矩"
                or self.control_page.current_torque_kind.currentData() == 1
            )
            return CmdId.CMD_CUR_TAU_MODE, codec.pack_current_torque(is_torque, value)
        if channel == "开环角度":
            duty = self.control_page.openloop_duty.value()
            return CmdId.CMD_OPENLOOP_THETA, codec.pack_openloop(duty, value)
        if channel == "开环速度":
            duty = self.control_page.openloop_duty.value()
            return CmdId.CMD_OPENLOOP_VEL, codec.pack_openloop(duty, value)
        if channel == "占空比":
            duty = value
            if self.control_page.openloop_kind.currentData() == 0:
                return CmdId.CMD_OPENLOOP_THETA, codec.pack_openloop(
                    duty, self.control_page.openloop_control.value())
            return CmdId.CMD_OPENLOOP_VEL, codec.pack_openloop(
                duty, self.control_page.openloop_control.value())
        return None, b""

    def _parameter_value(self, protocol_id, fallback=0.0):
        try:
            return float(self.config_page.get_parameter(protocol_id))
        except (KeyError, TypeError, ValueError):
            return fallback

    def _target_command(self, channel):
        return self._build_target_payload(channel, 0.0)[0]

    def _write_all_parameters(self):
        values = self._collect_config_values()
        records = [
            (meta, values.get(meta.id_name, meta.default))
            for meta in sorted(PARAMETERS.values(), key=lambda item: item.id)
        ]
        chunk = []
        payload_size = 2
        for meta, value in records:
            size = codec.value_size(meta)
            if chunk and payload_size + size > 128:
                self._send_payload(
                    CmdId.CMD_PARAM_ALLWRITE,
                    codec.pack_params_all_write(chunk),
                    f"批量写入 {chunk[0][0].id_name}..{chunk[-1][0].id_name}",
                )
                chunk = []
                payload_size = 2
            chunk.append((meta, value))
            payload_size += size
        if chunk:
            self._send_payload(
                CmdId.CMD_PARAM_ALLWRITE,
                codec.pack_params_all_write(chunk),
                f"批量写入 {chunk[0][0].id_name}..{chunk[-1][0].id_name}",
            )

    def _set_subscription(self):
        channels = []
        for channel_id in self._wave_map.values():
            if channel_id not in channels:
                channels.append(channel_id)
        channels = channels[:STREAM_MAX_CHANNELS]
        self._stream_channels = channels
        if self._connected:
            self._send_payload(
                CmdId.CMD_DATA_STREAM_SET,
                codec.pack_stream_set(channels),
                f"订阅示波数据流 {len(channels)} 通道",
            )

    def _handle_iap_action(self, action, payload):
        if action.startswith("firmware_") and action.endswith("_select"):
            route = action[len("firmware_"):-len("_select")]
            self._select_firmware(route)
        elif action.startswith("firmware_") and action.endswith("_烧录"):
            route = action[len("firmware_"):-len("_烧录")]
            self._burn_firmware(route)
        elif action.startswith("firmware_") and action.endswith("_取消"):
            if self._iap_worker is not None:
                self._iap_worker.stop()
                self._host("已请求取消固件升级")

    def _select_firmware(self, route):
        path, _ = QFileDialog.getOpenFileName(
            self.window, "选择固件文件", "", "Binary Files (*.bin);;All Files (*)")
        if not path:
            return
        self._firmware_files[route] = path
        self.iap_page.firmware_paths[route].setText(path)
        self._host(f"已选择 {route} 固件：{path}")

    def _burn_firmware(self, route):
        path = self._firmware_files.get(route)
        if not path:
            self._host("请先选择固件文件")
            return
        if self._iap_worker is not None and self._iap_worker.isRunning():
            self._host("已有升级任务在运行")
            return
        self._iap_route = route
        self.iap_page.set_progress(route, 0)
        worker = IapWorker(self.serial, path, self)
        worker.progress.connect(
            lambda value, _text, r=route: self.iap_page.set_progress(r, value))
        worker.log.connect(self._host)
        worker.result.connect(self._on_iap_finished)
        worker.finished.connect(
            lambda w=worker: self._on_iap_thread_finished(w))
        self._iap_worker = worker
        worker.start()
        self._host(f"开始烧录 {route}：{path}")

    def _on_iap_finished(self, success, message):
        route = self._iap_route
        if route:
            self.iap_page.set_progress(route, 100 if success else 0)
        self.iap_page.append_driver_log(message)
        self._host(message)
        self._iap_route = None

    def _on_iap_thread_finished(self, worker):
        if self._iap_worker is worker:
            self._iap_worker = None
        worker.deleteLater()

    # ---------- 本地配置 ----------
    def _refresh_configs(self, initial=False):
        names = self.config_store.names()
        current = self.config_page.config_file.currentText() or "default"
        if current not in names and names:
            current = "default" if "default" in names else names[0]
        self.config_page.set_config_files(names, current=current)
        if initial and "default" in names:
            self._load_config("default", announce=False)

    def _collect_config_values(self):
        values = {}
        for parameter in self.config_page.parameters.values():
            if parameter.read_only:
                continue
            meta = PARAMETERS.get(parameter.protocol_id)
            if not meta:
                continue
            try:
                values[meta.id_name] = parameter.value()
            except (TypeError, ValueError):
                continue
        return values

    def _load_config(self, name, announce=True):
        try:
            values = self.config_store.load(name)
        except Exception as error:
            self._host(f"加载配置失败：{error}")
            return
        applied = 0
        for id_name, value in values.items():
            meta = PARAM_BY_NAME.get(id_name)
            if not meta:
                continue
            if self.config_page.set_protocol_parameter(meta.id, value):
                applied += 1
        if announce:
            self._host(f"已加载配置 {name}：{applied} 项")

    def _save_config_as(self):
        current = self.config_page.config_file.currentText() or "default"
        dialog = TextInputDialog("保存配置", "配置名称：", current, self.window)
        if dialog.exec_() != QDialog.Accepted:
            return
        name = dialog.value()
        try:
            self.config_store.save(name, self._collect_config_values())
        except Exception as error:
            self._host(f"保存配置失败：{error}")
            return
        self._refresh_configs()
        index = self.config_page.config_file.findText(name)
        if index >= 0:
            self.config_page.config_file.setCurrentIndex(index)
        self._host(f"配置已保存：{name}")

    def _delete_config(self):
        name = self.config_page.config_file.currentText()
        if not name:
            return
        if name == "default":
            self._host("default 配置受保护，不能删除")
            return
        box = MessageBox("删除配置", f"确定删除配置“{name}”吗？", self.window)
        if box.exec_() != QDialog.Accepted:
            return
        try:
            self.config_store.delete(name)
        except Exception as error:
            self._host(f"删除配置失败：{error}")
            return
        self._refresh_configs()
        self._host(f"配置已删除：{name}")

    # ---------- 状态栏动作 ----------
    def _handle_status_action(self, action):
        if action == "system_reset":
            self._send_empty(CmdId.CMD_SYSTEM_RESET, "系统复位")
        elif action == "status_reset":
            self.status.update_status({
                "voltage": "—",
                "temperature": "—",
                "motor_temperature": "—",
                "state": "—",
                "error": "—",
            })
            self._host("状态显示已复位")

    # ---------- 发送辅助 ----------
    def _send_empty(self, cmd_id, description):
        return self._send_payload(cmd_id, b"", description)

    def _send_payload(self, cmd_id, payload, description):
        name = self._cmd_names.get(cmd_id, f"0x{cmd_id:02X}")
        if not self._connected:
            self._host(f"未连接，忽略：{description} ({name})")
            return False
        sent = self.serial.send(cmd_id, payload)
        self._host(f"TX {name}：{description} ({len(payload)}B)"
                   if sent else f"发送失败：{description}")
        return sent

    def _send_todo(self, cmd_id, description):
        name = self._cmd_names.get(cmd_id, f"0x{cmd_id:02X}" if isinstance(cmd_id, int) else str(cmd_id))
        message = f"TODO(payload)：{description} -> {name}"
        self._host(message)
        self._terminal(message)
        return False

    # ---------- 输出 ----------
    def _host(self, text):
        self.status.set_host_text(str(text))

    def _terminal(self, text):
        self.terminal.append_log(str(text))
