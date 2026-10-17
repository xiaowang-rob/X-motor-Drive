"""参数配置中心：顶层分类与分类页内导航。"""
from PyQt5.QtWidgets import QWidget, QHBoxLayout, QVBoxLayout, QStackedWidget
from qfluentwidgets import FluentIcon as FIF, HorizontalSeparator
from app.parameter_input import ParameterInput
from app.ui_components import preview_button
from qfluentwidgets.components.navigation import SegmentedWidget
from app.ui_components import (
    TabbedPage, SectionCard, SurfaceCard, CompactComboBox, icon_action,
)


class SideStack(QWidget):
    """使用 Fluent 分段控件的页内导航。"""
    def __init__(self, parent=None):
        super().__init__(parent)
        self.layout = QVBoxLayout(self)
        self.layout.setContentsMargins(0, 0, 0, 0)
        self.layout.setSpacing(8)
        self.nav = SegmentedWidget()
        self.nav.setObjectName("sideNavigation")
        self.stack = QStackedWidget()
        self.layout.addWidget(self.nav)
        self.layout.addWidget(self.stack, 1)
        self._routes = []
        self.nav.currentItemChanged.connect(self._on_route_changed)
        self.stack.currentChanged.connect(self._on_stack_changed)

    def add_page(self, name, page):
        route = f"side_{len(self._routes)}"
        self._routes.append(route)
        self.stack.addWidget(page)
        self.nav.addItem(route, name)
        if len(self._routes) == 1:
            self.nav.setCurrentItem(route)
            self.stack.setCurrentIndex(0)

    def _on_route_changed(self, route):
        if route in self._routes:
            self.stack.setCurrentIndex(self._routes.index(route))

    def _on_stack_changed(self, index):
        if 0 <= index < len(self._routes):
            self.nav.setCurrentItem(self._routes[index])


def section_page(*cards):
    page = QWidget()
    layout = QVBoxLayout(page)
    layout.setContentsMargins(0, 0, 0, 0)
    layout.setSpacing(12)
    for card in cards:
        layout.addWidget(card)
    layout.addStretch()
    return page


def calibration_row(scope, fields, *actions):
    """无标题校准卡片，参数与按钮处于同一行。"""
    widget = SurfaceCard()
    row = QHBoxLayout(widget)
    row.setContentsMargins(16, 14, 16, 16)
    row.setSpacing(8)
    for name, unit in fields:
        parameter = ParameterInput(name, f"{scope}/{name}", unit=unit, label_width=120)
        row.addWidget(parameter, 1)
    for text in actions:
        button = preview_button(text)
        button.setIcon(FIF.UPDATE)
        button.setProperty("calibrationScope", scope)
        row.addWidget(button)
    return widget


class ConfigPage(TabbedPage):
    def __init__(self):
        super().__init__("config_page")
        self.main_layout.removeWidget(self.pivot)
        self.main_layout.removeWidget(self.stack)
        body = QHBoxLayout()
        body.setSpacing(16)
        left = QWidget()
        left_layout = QVBoxLayout(left)
        left_layout.setContentsMargins(0, 0, 0, 0)
        left_layout.setSpacing(12)
        left_layout.addWidget(self.pivot)
        left_layout.addWidget(self.stack, 1)
        body.addWidget(left, 1)

        self.action_panel = SurfaceCard()
        self.action_panel.setFixedWidth(60)
        actions = QVBoxLayout(self.action_panel)
        actions.setContentsMargins(12, 12, 12, 12)
        actions.setSpacing(8)
        self.config_file = CompactComboBox()
        self.config_file.addItem("默认配置")
        self.config_file.setAccessibleName("配置文件")
        self.config_file.setToolTip("配置文件：" + self.config_file.currentText())
        self.config_file.currentTextChanged.connect(
            lambda text: self.config_file.setToolTip("配置文件：" + text))
        actions.addWidget(self.config_file)
        self.action_buttons = []
        for icon, text in [(FIF.FOLDER, "加载配置"), (FIF.SAVE_AS, "另存配置"),
                           (FIF.DELETE, "删除配置"), (None, ""),
                           (FIF.DOWNLOAD, "读取参数"), (FIF.UP, "写入参数"),
                           (FIF.SAVE, "保存参数"), (FIF.SYNC, "恢复默认参数")]:
            if icon is None:
                actions.addWidget(HorizontalSeparator())
                continue
            button = icon_action(icon, text)
            actions.addWidget(button)
            self.action_buttons.append(button)
        actions.addStretch()
        body.addWidget(self.action_panel)
        self.main_layout.addLayout(body, 1)

        self._build_motor()
        self._build_observer()
        self._build_controller()
        self._build_trajectory()
        self._build_protection()
        self.bind_parameters()
        for button, action in zip(self.action_buttons, (
                "load_config", "save_config_as", "delete_config", "read_parameters",
                "write_parameters", "save_to_device", "restore_defaults")):
            self.bind_action(button, action, lambda: self.config_file.currentText())
        for button in self.findChildren(QWidget):
            if button.property("calibrationScope"):
                self.bind_action(button, "calibrate", lambda b=button: {
                    "section": b.property("calibrationScope"), "operation": b.text()})

    def set_config_files(self, names, current=None):
        self.config_file.clear()
        self.config_file.addItems(list(names))
        if current is not None:
            self.config_file.setCurrentText(current)

    def _build_motor(self):
        motor = self.add_tab("motor", "电机")
        self.motor_nav = SideStack()
        motor_common = SectionCard("通用配置")
        motor_common.add_fields([("电机类型", ["PMSM", "BLDC"]), 
                                 ("极对数", ""),
                                 ("KV 值", "rpm/V"), 
                                 ("额定电流", "A"),
                                 ("转动惯量(J)", "kg·m²"), 
                                 ("粘性摩擦系数(B)", "N·m·s/rad"),
                                 ])

        self.motor_nav.add_page("通用", section_page(motor_common))
        pmsm = SectionCard("")
        pmsm.add_fields([("相电阻(Rs)", "Ω"), ("d 轴电感(Ld)", "mH"),
                         ("q 轴电感(Lq)", "mH"), ("磁链(Psif)", "Wb"),
                         ("反电动势常数(Ke)", "V/(rad/s mech)")])
        foc_config = SectionCard("")
        foc_config.add_fields([("电流环带宽系数", "0~1"), ("d轴电流环带宽", "Hz"),
                               ("d轴电流环Kp", ""), ("d轴电流环Ki", ""),
                               ("q轴电流环带宽", "Hz"), ("q轴电流环Kp", ""),
                               ("q轴电流环Ki", ""), ("电流滤波系数", "0~1")])
        self.parameter_card = pmsm
        for name, parameter in pmsm.parameters.items():
            legacy_name = {"相电阻(Rs)": "相电阻", "磁链(Psif)": "磁链"}.get(name, name)
            parameter.param_id = f"FOC 整定/{legacy_name}"
        self.pmsm_scroll, pmsm_layout = self.add_scroll("motor_pmsm")
        pmsm_layout.addWidget(pmsm)
        pmsm_layout.addWidget(foc_config)
        pmsm_layout.addStretch()
        self.motor_nav.add_page("PMSM", self.pmsm_scroll)

        bldc = SectionCard("")
        bldc.parameter_scope = "BLDC"
        self.motor_nav.add_page("BLDC", section_page(bldc))
        motor.addWidget(self.motor_nav, 1)
        self.motor_calibration_row = calibration_row(
            "电机校准", [("校准电流", "A"), ("校准速度", "rad/s")],
            "RL校准", "Psif校准", "机械校准")
        motor.addWidget(self.motor_calibration_row)

    def _motor_pmsm(self):
        card = SectionCard("pmsm电机参数")
        card.add_fields([("相电阻(Rs)", "Ω"), ("d 轴电感(Ld)", "mH"),
                         ("q 轴电感(Lq)", "mH"), ("磁链(Psif)", "Wb"),
                         ("反电动势常数(Ke)", "V/(rad/s mech)")])
        return card

    def _build_observer(self):
        observer = self.add_tab("observer", "观测器")
        body = QVBoxLayout()
        self.observer_nav = SideStack()
        self.observer_nav.add_page("通用", section_page(self._observer_general()))
        self.observer_nav.add_page("无感 观测器", section_page(self._observer_sensorless()))
        self.observer_nav.add_page("HALL 编码器", section_page(self._observer_enc_hall()))
        self.observer_nav.add_page("ABI 编码器", section_page(self._observer_enc_abi()))
        self.observer_nav.add_page("SIN/COS 编码器", section_page(self._observer_enc_sincos()))
        self.observer_nav.add_page("SPI 编码器", section_page(self._observer_enc_spi()))
        body.addWidget(self.observer_nav, 1)
        self.observer_calibration_row = calibration_row(
            "观测器校准", [("校准电流", "A")], "开始校准", "设置零位")
        body.addWidget(self.observer_calibration_row)
        observer.addLayout(body)

    def _observer_general(self):
        card = SectionCard("")
        card.add_fields([("转子观测器", ["内置", "无感观测器", "HALL编码器", "ABI编码器", "SINCOS编码器", "SPI编码器"]),
                         ("绝对位置观测器", ["禁用", "内置", "HALL编码器", "ABI编码器", "SINCOS编码器", "SPI编码器"]),
                         ("零位偏置", "rad"), ("方向反转", ["false", "true"]),
                         ("角度源", ["PLL", "RAW"]), ("速度源", ["PLL", "FDM(差分速度)"])])
        return card

    def _observer_sensorless(self):
        card = SectionCard("")
        card.add_fields([("阈值电频率", "Hz")])
        return card

    def _observer_enc_hall(self):
        card = SectionCard("")
        card.add_fields([(f"TABLE[{i}]", "") for i in range(8)])
        return card

    def _observer_enc_abi(self):
        card = SectionCard("")
        card.add_fields([("计数", "")])
        return card

    def _observer_enc_sincos(self):
        card = SectionCard("")
        card.add_fields([("sin幅值", "V"), ("cos幅值", "V"), ("sin偏置", "V"),
                         ("cos偏置", "V"), ("滤波系数", "0~1")])
        return card

    def _observer_enc_spi(self):
        card = SectionCard("")
        card.add_fields([("芯片", ["MT6816", "MT6835", "AS5047"])])
        return card

    def _build_controller(self):
        controller = self.add_tab("controller", "控制器")
        body = QVBoxLayout()
        self.controller_nav = SideStack()
        pid = SectionCard("PID 参数")
        pid.add_fields([("速度环 Kp", ""), ("速度环 Ki", ""),
                        ("位置环模式", ["PVC（三环级联）", "PC（双环）"]),
                        ("位置环 Kp", ""), ("位置环 Ki", ""), ("位置环 Kd", ""), ("滤波系数", "0~1")])
        mit = SectionCard("MIT 参数")
        mit.add_fields([("刚度 Kp", "N·m/rad"), ("阻尼 Kd", "N·m·s/rad"),
                        ("前馈转矩", "N·m"), ("转矩限幅", "N·m")])
        advanced = SectionCard("高级")
        advanced.add_fields([("弱磁环 Kp", ""), ("弱磁环 Ki", "")])


        self.controller_nav.add_page("PID", section_page(pid))
        self.controller_nav.add_page("MIT", section_page(mit))
        self.controller_nav.add_page("高级", section_page(advanced))

        body.addWidget(self.controller_nav, 1)
        self.controller_calibration_row = QWidget()
        calibration = QHBoxLayout(self.controller_calibration_row)
        calibration.setContentsMargins(0, 0, 0, 0)
        calibration.setSpacing(12)
        calibration.addWidget(calibration_row(
            "PID校准", [("占位参数", "")], "PID校准"), 1)
        calibration.addWidget(calibration_row(
            "MIT校准", [("占位参数", "")], "MIT校准"), 1)
        body.addWidget(self.controller_calibration_row)
        controller.addLayout(body)

    def _build_trajectory(self):
        trajectory = self.add_tab("trajectory", "轨迹规划器")
        planner = SectionCard("轨迹规划")
        planner.add_fields([("规划模式", ["禁用", "梯形", "S 曲线", "PVT"]), ("一阶限幅", ""),
                            ("二阶限幅", ""), ("三阶限幅", ""), ("到位容差", "")])
        trajectory.addWidget(planner)

    def _build_protection(self):
        protection = self.add_tab("protection", "保护")
        self.protection_nav = SideStack()
        electrical = SectionCard("电气保护")
        electrical.add_fields([("欠压阈值", "V"), ("过压阈值", "V"), ("过流阈值", "A"),
                            ("故障容忍度", "0-1"),("故障容忍时间", "ms")])
        motion = SectionCard("运动保护")
        motion.add_fields([("速度限制", "rad/s"), ("最小位置", "rad"), ("最大位置", "rad"),
                           ("转矩限幅", "N·m"), ("超限容忍度", "0-1"),("超限容忍时间", "ms")])
        self.protection_nav.add_page("电气保护", section_page(electrical))
        self.protection_nav.add_page("运动保护", section_page(motion))
        protection.addWidget(self.protection_nav)
