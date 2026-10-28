"""参数配置中心：协议驱动的参数卡片与固定校准栏。"""

from PyQt5.QtWidgets import QWidget, QHBoxLayout, QVBoxLayout, QStackedWidget
from qfluentwidgets import CaptionLabel, FluentIcon as FIF, HorizontalSeparator
from qfluentwidgets.components.navigation import SegmentedWidget

from protocol import CmdId, Pidx
from app.calibration_card import CalibrationCard, CalibrationSpec
from app.ui_components import (
    TabbedPage,
    PlainCard,
    ScrollCard,
    SurfaceCard,
    CompactComboBox,
    icon_action,
)

# ============================================================
# 参数页面分组：字段顺序、页面名称都在这里配置。
# 字段本身的名称/单位/默认值/选项/类型由 protocol 包提供。
# ============================================================
MOTOR_PARAMS = (
    Pidx.PARAM_MOTOR_TYPE,
    Pidx.PARAM_MOTOR_POLEPAIRS,
    Pidx.PARAM_MOTOR_KV,
    Pidx.PARAM_MOTOR_RATED_CURRENT,
    Pidx.PARAM_MOTOR_RS,
    Pidx.PARAM_MOTOR_LD,
    Pidx.PARAM_MOTOR_LQ,
    Pidx.PARAM_MOTOR_PSIF,
    Pidx.PARAM_MOTOR_KE,
    Pidx.PARAM_MOTOR_J,
    Pidx.PARAM_MOTOR_B,
    Pidx.PARAM_CLBW_COEF,
    Pidx.PARAM_DCLBW,
    Pidx.PARAM_DCLKP,
    Pidx.PARAM_DCLKI,
    Pidx.PARAM_QCLBW,
    Pidx.PARAM_QCLKP,
    Pidx.PARAM_QCLKI,
    Pidx.PARAM_CFALPHA,
)

OBSERVER_PAGES = (
    (
        "通用",
        (
            Pidx.PARAM_ROTOR_OBS,
            Pidx.PARAM_ABS_POS_OBS,
            Pidx.PARAM_THETA_OFFSET,
            Pidx.PARAM_POSITIVE_DIR,
            Pidx.PARAM_THETA_SOURCE,
            Pidx.PARAM_VEL_SOURCE,
        ),
    ),
    ("无感观测器", (Pidx.PARAM_HFI_SMO_THRESHOLD,)),
    (
        "HALL 编码器",
        (
            Pidx.PARAM_HALL_TABLE_0,
            Pidx.PARAM_HALL_TABLE_1,
            Pidx.PARAM_HALL_TABLE_2,
            Pidx.PARAM_HALL_TABLE_3,
            Pidx.PARAM_HALL_TABLE_4,
            Pidx.PARAM_HALL_TABLE_5,
            Pidx.PARAM_HALL_TABLE_6,
            Pidx.PARAM_HALL_TABLE_7,
        ),
    ),
    ("ABI 编码器", (Pidx.PARAM_ABI_COUNT,)),
    (
        "SIN/COS 编码器",
        (
            Pidx.PARAM_SIN_AMP,
            Pidx.PARAM_COS_AMP,
            Pidx.PARAM_SIN_OFFSET,
            Pidx.PARAM_COS_OFFSET,
            Pidx.PARAM_SINCOS_FL,
        ),
    ),
    ("SPI 编码器", (Pidx.PARAM_ENC_CHIP,)),
)

CONTROLLER_PAGES = (
    (
        "PID",
        (
            Pidx.PARAM_VLKP,
            Pidx.PARAM_VLKI,
            Pidx.PARAM_VELOCITY_LIMIT,
            Pidx.PARAM_POS_PIDMODE,
            Pidx.PARAM_PLKP,
            Pidx.PARAM_PLKI,
            Pidx.PARAM_PLKD,
            Pidx.PARAM_PLALPHA,
            Pidx.PARAM_POS_LIMIT_MODE,
            Pidx.PARAM_POS_MIN_LIMIT,
            Pidx.PARAM_POS_MAX_LIMIT,
        ),
    ),
    (
        "MIT",
        (
            Pidx.PARAM_MIT_KP,
            Pidx.PARAM_MIT_KD,
            Pidx.PARAM_MIT_KPMAX,
            Pidx.PARAM_MIT_KDMAX,
            Pidx.PARAM_MIT_TMAX,
        ),
    ),
    ("高级", (Pidx.PARAM_FWLKP, Pidx.PARAM_FWLKI)),
)

TRAJECTORY_PARAMS = (
    Pidx.PARAM_TRAJ_TYPE,
    Pidx.PARAM_TRAJ_LIMIT_D1,
    Pidx.PARAM_TRAJ_LIMIT_D2,
    Pidx.PARAM_TRAJ_LIMIT_D3,
    Pidx.PARAM_TRAJ_TOLERANCE,
)

COMMUNICATION_PARAMS = (
    Pidx.PARAM_CAN_RATE,
    Pidx.PARAM_CAN_ID,
    Pidx.PARAM_CAN_MODE,
)

PROTECTION_PAGES = (
    Pidx.PARAM_UNDERVOLTAGE_THRESHOLD,
    Pidx.PARAM_OVERVOLTAGE_THRESHOLD,
    Pidx.PARAM_OVERTEMP_THRESHOLD,
    Pidx.PARAM_PRO_TOLERANCE,
    Pidx.PARAM_PRO_TOLERANCE_TIME_MS,
)

# 校准卡片配置：fields 为协议参数，actions 为 (命令 ID, 按钮文字)。
# help_text 可以直接改成自己的说明，点击 info 按钮弹窗显示。
CALIBRATIONS = {
    "motor": CalibrationSpec(
        scope="电机校准",
        fields=(Pidx.PARAM_TUNE_CURRENT, Pidx.PARAM_TUNE_VEL),
        actions=(
            (CmdId.CMD_RL_TUNNING, "RL校准"),
            (CmdId.CMD_PSIF_TUNNING, "Psif校准"),
            (CmdId.CMD_MECH_TUNNING, "机械校准"),
        ),
        help_text=(
            "RL 校准：测量相电阻与电感。\n"
            "Psif 校准：测量永磁体磁链。\n"
            "机械校准：测量转动惯量与阻尼。\n"
            "请按实际电机状态填写校准电流/速度后执行。"
        ),
    ),
    "observer": CalibrationSpec(
        scope="观测器校准",
        fields=(Pidx.PARAM_TUNE_CURRENT,),
        actions=(
            (CmdId.CMD_OBS_TUNNING, "观测器校准"),
            (CmdId.CMD_SET_ZERO_POS, "设置零位"),
        ),
        help_text=(
            "观测器校准前请确认编码器或霍尔安装正确。\n"
            "设置零位会把当前位置写入零点。"
        ),
    ),
    # PID/MIT 目前没有独立校准参数；需要时在 fields 中加入 Pidx.PARAM_*。
    "pid": CalibrationSpec(
        scope="PID校准",
        actions=((CmdId.CMD_PID_TUNNING, "PID校准"),),
        help_text="PID 校准说明请在此编辑。",
    ),
    "mit": CalibrationSpec(
        scope="MIT校准",
        actions=((CmdId.CMD_MIT_TUNNING, "MIT校准"),),
        help_text="MIT 校准说明请在此编辑。",
    ),
}


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


def page_with(*cards):
    """把卡片放进页内导航的页面；卡片占满剩余高度。"""
    page = QWidget()
    layout = QVBoxLayout(page)
    layout.setContentsMargins(0, 0, 0, 0)
    layout.setSpacing(12)
    for card in cards:
        layout.addWidget(card, 1)
    return page


def section_page(*cards):
    """兼容旧代码的普通页面包装。"""
    page = QWidget()
    layout = QVBoxLayout(page)
    layout.setContentsMargins(0, 0, 0, 0)
    layout.setSpacing(12)
    for card in cards:
        layout.addWidget(card)
    layout.addStretch()
    return page


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
            lambda text: self.config_file.setToolTip("配置文件：" + text)
        )
        actions.addWidget(self.config_file)
        self.action_buttons = []
        for icon, text in [
            (FIF.FOLDER, "加载配置"),
            (FIF.SAVE_AS, "另存配置"),
            (FIF.DELETE, "删除配置"),
            (None, ""),
            (FIF.DOWNLOAD, "读取参数"),
            (FIF.UP, "写入参数"),
            (FIF.SAVE, "保存参数"),
            (FIF.SYNC, "恢复默认参数"),
        ]:
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
        self._build_communication()
        self._build_protection()
        self.bind_parameters()
        for button, action in zip(
            self.action_buttons,
            (
                "load_config",
                "save_config_as",
                "delete_config",
                "read_parameters",
                "write_parameters",
                "save_to_device",
                "restore_defaults",
            ),
        ):
            self.bind_action(button, action, lambda: self.config_file.currentText())
        for card in self.findChildren(CalibrationCard):
            card.action_requested.connect(self.action_requested)

    def set_config_files(self, names, current=None):
        self.config_file.clear()
        self.config_file.addItems(list(names))
        if current is not None:
            self.config_file.setCurrentText(current)

    def _build_motor(self):
        layout = self.add_tab("motor", "电机")
        self.motor_scroll = ScrollCard("电机参数")
        self.motor_scroll.add_protocol_fields(*MOTOR_PARAMS)
        layout.addWidget(self.motor_scroll, 1)
        self.motor_calibration = CalibrationCard(CALIBRATIONS["motor"])
        layout.addWidget(self.motor_calibration)

    def _build_observer(self):
        layout = self.add_tab("observer", "观测器")
        self.observer_nav = SideStack()
        for title, params in OBSERVER_PAGES:
            card = ScrollCard(title)
            card.add_protocol_fields(*params)
            self.observer_nav.add_page(title, page_with(card))
        layout.addWidget(self.observer_nav, 1)
        self.observer_calibration = CalibrationCard(CALIBRATIONS["observer"])
        layout.addWidget(self.observer_calibration)

    def _build_controller(self):
        layout = self.add_tab("controller", "控制器")
        self.controller_nav = SideStack()
        for title, params in CONTROLLER_PAGES:
            card = ScrollCard(title)
            card.add_protocol_fields(*params)
            self.controller_nav.add_page(title, page_with(card))
        layout.addWidget(self.controller_nav, 1)

        calibration_row = QHBoxLayout()
        calibration_row.setSpacing(12)
        self.pid_calibration = CalibrationCard(CALIBRATIONS["pid"])
        self.mit_calibration = CalibrationCard(CALIBRATIONS["mit"])
        calibration_row.addWidget(self.pid_calibration, 1)
        calibration_row.addWidget(self.mit_calibration, 1)
        layout.addLayout(calibration_row)

    def _build_trajectory(self):
        layout = self.add_tab("trajectory", "轨迹规划器")
        self.trajectory_scroll = ScrollCard("轨迹规划")
        self.trajectory_scroll.add_protocol_fields(*TRAJECTORY_PARAMS)
        layout.addWidget(self.trajectory_scroll, 1)

    def _build_communication(self):
        layout = self.add_tab("communication", "通讯")
        self.can_card = ScrollCard("CAN")
        self.can_card.add_protocol_fields(*COMMUNICATION_PARAMS)
        layout.addWidget(self.can_card, 1)
        self.rs485_card = PlainCard("RS485")
        self.rs485_card.content.addWidget(CaptionLabel("RS485（预留）"))
        layout.addWidget(self.rs485_card)

    def _build_protection(self):
        layout = self.add_tab("protection", "保护")
        self.protection_scroll = ScrollCard("保护")
        self.protection_scroll.add_protocol_fields(*PROTECTION_PAGES)
        layout.addWidget(self.protection_scroll, 1)
