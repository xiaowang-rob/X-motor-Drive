from PyQt5.QtWidgets import QWidget, QHBoxLayout, QVBoxLayout, QScrollArea, QGridLayout
from PyQt5.QtCore import Qt
from qfluentwidgets import (
    PushButton, ComboBox, LineEdit,
    BodyLabel, StrongBodyLabel, ScrollArea, CaptionLabel, HorizontalSeparator
)
from protocol import Midx
from app.ui_components import SurfaceCard


class DataPage(QWidget):
    """数据参数页面 —— 控制/模式/电机参数 + 日志显示"""

    def __init__(self):
        super().__init__()
        self.setObjectName("data_page")

        main_layout = QHBoxLayout(self)
        main_layout.setContentsMargins(12, 8, 12, 8)
        main_layout.setSpacing(12)

        # ── 1. 控制参数卡片 ──
        self.control_card = SurfaceCard()
        ctrl_container = QWidget()
        control_layout = QVBoxLayout(ctrl_container)
        control_layout.setContentsMargins(8, 4, 8, 8)
        control_layout.setSpacing(12)

        self.all_read_button = PushButton()
        self.all_read_button.setText("一键读取")
        self.all_write_button = PushButton()
        self.all_write_button.setText("一键写入")
        btn_row1 = QHBoxLayout()
        btn_row1.setContentsMargins(0, 0, 0, 0)
        btn_row1.setSpacing(8)
        btn_row1.addWidget(self.all_read_button)
        btn_row1.addWidget(self.all_write_button)

        self.all_save_button = PushButton()
        self.all_save_button.setText("一键保存")
        self.all_erase_button = PushButton()
        self.all_erase_button.setText("清除参数")
        self.all_erase_button.setToolTip("清除所有参数")
        btn_row2 = QHBoxLayout()
        btn_row2.setContentsMargins(0, 0, 0, 0)
        btn_row2.setSpacing(8)
        btn_row2.addWidget(self.all_save_button)
        btn_row2.addWidget(self.all_erase_button)

        control_layout.addLayout(btn_row1)
        control_layout.addLayout(btn_row2)

        # 可滚动的参数区域
        scroll = ScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setStyleSheet("ScrollArea{background:transparent;border:none}")
        scroll.viewport().setStyleSheet("background:transparent")
        scroll_container = QWidget()
        scroll_container.setStyleSheet("background:transparent")
        scroll_layout = QVBoxLayout(scroll_container)
        scroll_layout.setContentsMargins(0, 0, 0, 0)
        scroll_layout.setSpacing(6)
        scroll_layout.setAlignment(Qt.AlignTop)

        self.CAN_ID_input = LineEdit()
        self.CAN_ID_input.setPlaceholderText("0")
        self._add_param_row(scroll_layout, "CAN ID", self.CAN_ID_input)

        # PID 参数
        self._add_section_title(scroll_layout, "PID参数")
        self.speed_loop_P_input = LineEdit()
        self.speed_loop_P_input.setPlaceholderText("0")
        self._add_param_row(scroll_layout, "速度环 p", self.speed_loop_P_input)
        self.speed_loop_I_input = LineEdit()
        self.speed_loop_I_input.setPlaceholderText("0")
        self._add_param_row(scroll_layout, "速度环 i", self.speed_loop_I_input)
        self.position_loop_P_input = LineEdit()
        self.position_loop_P_input.setPlaceholderText("0")
        self._add_param_row(scroll_layout, "位置环 p", self.position_loop_P_input)
        self.position_loop_I_input = LineEdit()
        self.position_loop_I_input.setPlaceholderText("0")
        self._add_param_row(scroll_layout, "位置环 i", self.position_loop_I_input)
        self.position_loop_D_input = LineEdit()
        self.position_loop_D_input.setPlaceholderText("0")
        self._add_param_row(scroll_layout, "位置环 d", self.position_loop_D_input)
        self.position_loop_alpha_input = LineEdit()
        self.position_loop_alpha_input.setPlaceholderText("0")
        self._add_param_row(scroll_layout, "滤波系数", self.position_loop_alpha_input, unit="0~1")

        # MIT 参数
        self._add_section_title(scroll_layout, "MIT参数")
        self.mit_kp_input = LineEdit()
        self.mit_kp_input.setPlaceholderText("0")
        self._add_param_row(scroll_layout, "刚度 kp", self.mit_kp_input)
        self.mit_kd_input = LineEdit()
        self.mit_kd_input.setPlaceholderText("0")
        self._add_param_row(scroll_layout, "阻尼 kd", self.mit_kd_input)
        self.mit_tau_max_input = LineEdit()
        self.mit_tau_max_input.setPlaceholderText("100")
        self._add_param_row(scroll_layout, "最大扭矩", self.mit_tau_max_input)

        # 轨迹规划参数
        self._add_section_title(scroll_layout, "轨迹规划参数")
        self.traj_limit_d1 = LineEdit()
        self.traj_limit_d1.setPlaceholderText("100")
        self._add_param_row(scroll_layout, "一阶限幅", self.traj_limit_d1)
        self.traj_limit_d2 = LineEdit()
        self.traj_limit_d2.setPlaceholderText("50")
        self._add_param_row(scroll_layout, "二阶限幅", self.traj_limit_d2)
        self.traj_limit_d3 = LineEdit()
        self.traj_limit_d3.setPlaceholderText("200")
        self._add_param_row(scroll_layout, "三阶限幅", self.traj_limit_d3)
        self.traj_tolerance = LineEdit()
        self.traj_tolerance.setPlaceholderText("0.01")
        self._add_param_row(scroll_layout, "容差", self.traj_tolerance)

        # 安全参数
        self._add_section_title(scroll_layout, "安全参数")
        self.tune_current = LineEdit()
        self.tune_current.setPlaceholderText("50")
        self._add_param_row(scroll_layout, "校准电流", self.tune_current, unit="A")
        self.limit_current = LineEdit()
        self.limit_current.setPlaceholderText("50")
        self._add_param_row(scroll_layout, "电流限幅", self.limit_current, unit="A")
        self.limit_speed = LineEdit()
        self.limit_speed.setPlaceholderText("104.7")
        self._add_param_row(scroll_layout, "速度限幅", self.limit_speed, unit="rad/s")
        self.min_position = LineEdit()
        self.min_position.setPlaceholderText("-174.5")
        self._add_param_row(scroll_layout, "最小位置", self.min_position, unit="rad")
        self.max_position = LineEdit()
        self.max_position.setPlaceholderText("174.5")
        self._add_param_row(scroll_layout, "最大位置", self.max_position, unit="rad")

        # 容错参数
        self._add_section_title(scroll_layout, "容错参数")
        self.tolerance_time = LineEdit()
        self.tolerance_time.setPlaceholderText("500")
        self._add_param_row(scroll_layout, "容错时间", self.tolerance_time, unit="ms")
        self.tolerance_limit = LineEdit()
        self.tolerance_limit.setPlaceholderText("0")
        self._add_param_row(scroll_layout, "容错限幅", self.tolerance_limit)

        scroll.setWidget(scroll_container)
        control_layout.addWidget(scroll)
        QVBoxLayout(self.control_card).addWidget(ctrl_container)

        # ── 2. 模式参数卡片 ──
        self.mode_card = SurfaceCard()
        mode_container = QWidget()
        mode_layout = QVBoxLayout(mode_container)
        mode_layout.setContentsMargins(8, 4, 8, 8)
        mode_layout.setSpacing(12)

        self.encoder_input = ComboBox()
        self.encoder_input.addItems(Midx.encoder_chip)
        self._add_param_row(mode_layout, "编码器芯片", self.encoder_input)
        self.sensormode_input = ComboBox()
        self.sensormode_input.addItems(Midx.sensor_mode)
        self._add_param_row(mode_layout, "感应模式", self.sensormode_input)
        self.runmode_input = ComboBox()
        self.runmode_input.addItems(Midx.run_mode)
        self._add_param_row(mode_layout, "运行模式", self.runmode_input)
        self.can_mode_input = ComboBox()
        self.can_mode_input.addItems(Midx.can_mode)
        self._add_param_row(mode_layout, "CAN模式", self.can_mode_input)
        self.vaguePID_input = ComboBox()
        self.vaguePID_input.addItems(Midx.vague_PID_mode)
        self._add_param_row(mode_layout, "模糊PID", self.vaguePID_input)
        self.PVT_mode_input = ComboBox()
        self.PVT_mode_input.addItems(Midx.pvt_mode)
        self._add_param_row(mode_layout, "PVT模式", self.PVT_mode_input)
        self.TRAJ_mode_input = ComboBox()
        self.TRAJ_mode_input.addItems(Midx.traj_type)
        self._add_param_row(mode_layout, "轨迹模式", self.TRAJ_mode_input)
        mode_layout.addStretch()
        QVBoxLayout(self.mode_card).addWidget(mode_container)

        # ── 3. 电机参数卡片 ──
        self.motor_card = SurfaceCard()
        motor_container = QWidget()
        motor_layout = QVBoxLayout(motor_container)
        motor_layout.setContentsMargins(8, 4, 8, 8)
        motor_layout.setSpacing(12)

        self.offsetangle_input = LineEdit()
        self.offsetangle_input.setPlaceholderText("0")
        self._add_param_row(motor_layout, "偏置角", self.offsetangle_input, unit="rad")
        self.motor_polepairs_input = LineEdit()
        self.motor_polepairs_input.setPlaceholderText("7")
        self._add_param_row(motor_layout, "极对数", self.motor_polepairs_input)
        self.motor_KV_input = LineEdit()
        self.motor_KV_input.setPlaceholderText("100")
        self._add_param_row(motor_layout, "KV值", self.motor_KV_input, unit="rpm/V")
        self.motor_resistance_input = LineEdit()
        self.motor_resistance_input.setPlaceholderText("0.01")
        self._add_param_row(motor_layout, "电阻", self.motor_resistance_input, unit="Ω")
        self.motor_Ld_input = LineEdit()
        self.motor_Ld_input.setPlaceholderText("0.001")
        self._add_param_row(motor_layout, "d轴电感", self.motor_Ld_input, unit="mH")
        self.motor_Lq_input = LineEdit()
        self.motor_Lq_input.setPlaceholderText("0.001")
        self._add_param_row(motor_layout, "q轴电感", self.motor_Lq_input, unit="mH")
        self.motor_psif_input = LineEdit()
        self.motor_psif_input.setPlaceholderText("0.001")
        self._add_param_row(motor_layout, "磁链", self.motor_psif_input, unit="Wb")
        self.motor_Ke_input = LineEdit()
        self.motor_Ke_input.setPlaceholderText("0.001")
        self._add_param_row(motor_layout, "反电动势", self.motor_Ke_input, unit="V/(rad/s)")
        self.motor_J_input = LineEdit()
        self.motor_J_input.setPlaceholderText("0.001")
        self._add_param_row(motor_layout, "转动惯量", self.motor_J_input, unit="kg·m²")
        self.motor_B_input = LineEdit()
        self.motor_B_input.setPlaceholderText("0.001")
        self._add_param_row(motor_layout, "摩擦系数", self.motor_B_input, unit="N·m·s")
        motor_layout.addStretch()
        QVBoxLayout(self.motor_card).addWidget(motor_container)

        # ── 4. 日志信息卡片 ──
        self.log_card = SurfaceCard()
        log_container = QWidget()
        log_layout = QVBoxLayout(log_container)
        log_layout.setContentsMargins(8, 4, 8, 8)
        log_layout.setSpacing(12)

        self.read_log_btn = PushButton()
        self.read_log_btn.setText("读取日志")
        self.clear_log_btn = PushButton()
        self.clear_log_btn.setText("清空日志")
        self.clear_log_btn.setToolTip("清空日志")
        log_btn_row = QHBoxLayout()
        log_btn_row.setContentsMargins(0, 0, 0, 0)
        log_btn_row.setSpacing(8)
        log_btn_row.addWidget(self.read_log_btn)
        log_btn_row.addWidget(self.clear_log_btn)
        log_layout.addLayout(log_btn_row)

        self.log_num = ComboBox()
        self.log_num.setPlaceholderText("选择日志")
        self.show_log_btn = PushButton()
        self.show_log_btn.setText("▶")
        self.show_log_btn.setToolTip("显示日志")
        log_select_row = QHBoxLayout()
        log_select_row.setContentsMargins(0, 0, 0, 0)
        log_select_row.setSpacing(8)
        log_select_row.addWidget(self.log_num, 2)
        log_select_row.addWidget(self.show_log_btn, 1)
        log_layout.addLayout(log_select_row)

        # 日志详情滚动区
        log_scroll = ScrollArea()
        log_scroll.setWidgetResizable(True)
        log_scroll.setStyleSheet("ScrollArea{background:transparent;border:none}")
        log_scroll.viewport().setStyleSheet("background:transparent")
        log_container_w = QWidget()
        log_container_w.setStyleSheet("background:transparent")
        log_container_layout = QVBoxLayout(log_container_w)
        log_container_layout.setContentsMargins(0, 0, 0, 0)
        log_container_layout.setSpacing(4)
        log_container_layout.setAlignment(Qt.AlignTop)

        log_fields = [
            ("序号", "num"), ("发生时间", "time", "分"), ("错误", "fault"),
            ("警告", "warning"), ("感应模式", "sensor_mode"), ("运行模式", "run_mode"),
            ("CAN状态", "can_status"), ("编码状态", "encode_status"),
            ("电压", "voltage", "V"), ("温度", "temperature", "°C"),
            ("相电流 U", "iu", "A"), ("相电流 V", "iv", "A"), ("相电流 W", "iw", "A"),
            ("d轴电流", "id", "A"), ("d轴电流目标值", "id_ref", "A"),
            ("q轴电流", "iq", "A"), ("q轴电流目标值", "iq_ref", "A"),
            ("速度", "speed", "rad/s"), ("目标速度", "target_speed", "rad/s"),
            ("位置", "position", "rad"), ("目标位置", "target_position", "rad"),
        ]
        for item in log_fields:
            label = item[0]
            attr = item[1]
            unit = item[2] if len(item) > 2 else None
            edit = LineEdit()
            edit.setReadOnly(True)
            edit.setPlaceholderText(label)
            setattr(self, attr, edit)
            self._add_param_row(log_container_layout, label, edit, unit=unit)

        log_scroll.setWidget(log_container_w)
        log_layout.addWidget(log_scroll)
        QVBoxLayout(self.log_card).addWidget(log_container)

        # ── 组装 ──
        grid = QHBoxLayout()
        grid.setContentsMargins(0, 0, 0, 0)
        grid.setSpacing(12)
        grid.addWidget(self.control_card, 1)
        grid.addWidget(self.mode_card, 1)
        grid.addWidget(self.motor_card, 1)
        grid.addWidget(self.log_card, 1)
        main_layout.addLayout(grid)

    def _add_param_row(self, layout, label_text, widget, unit=None, label_stretch=2, input_stretch=3, unit_stretch=1):
        row = QHBoxLayout()
        row.setContentsMargins(0, 0, 0, 0)
        row.setSpacing(6)
        lbl = BodyLabel(label_text)
        lbl.setStyleSheet("font-size:8px;")
        row.addWidget(lbl, label_stretch)
        row.addWidget(widget, input_stretch)
        # 单位标签（无单位时显示空格保持对齐）
        unit_lbl = CaptionLabel(unit if unit else " ")
        row.addWidget(unit_lbl, unit_stretch)
        layout.addLayout(row)

    def _add_section_title(self, layout, text):
        """添加分段标题（加粗+分隔线，与参数行区分）"""
        row = QHBoxLayout()
        row.setContentsMargins(0, 6, 0, 2)
        lbl = StrongBodyLabel(text)
        lbl.setStyleSheet("font-size:16px; padding:2px 0;")
        row.addWidget(lbl)
        layout.addLayout(row)
        sep = HorizontalSeparator()
        sep.setFixedHeight(2)
        layout.addWidget(sep)
