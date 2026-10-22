# 此文件由 protocol/gen_protocol.py 自动生成，请勿手动修改

class eFieldValueType:
    FLD_U8 = 0  # uint8_t
    FLD_U16 = 1  # uint16_t
    FLD_U32 = 2  # uint32_t
    FLD_F32 = 3  # float
    FLD_BOOL = 4  # bool

FieldValueType = eFieldValueType

class Pidx:
    PARAM_MOTOR_TYPE = 0x01  # 电机类型
    PARAM_MOTOR_POLEPAIRS = 0x02  # 极对数
    PARAM_MOTOR_KV = 0x03  # 电机KV值
    PARAM_MOTOR_RATED_CURRENT = 0x04  # 额定电流
    PARAM_MOTOR_RS = 0x05  # 定子电阻(Rs)
    PARAM_MOTOR_LD = 0x06  # d轴电感(Ld)
    PARAM_MOTOR_LQ = 0x07  # q轴电感(Lq)
    PARAM_MOTOR_PSIF = 0x08  # 永磁体磁链(λ)
    PARAM_MOTOR_KE = 0x09  # 反电动势常数(Ke)
    PARAM_MOTOR_J = 0x0A  # 转动惯量(J)
    PARAM_MOTOR_B = 0x0B  # 粘滞摩擦系数(B)
    PARAM_CLBW_COEF = 0x0C  # 电流环带宽系数
    PARAM_DCLBW = 0x0D  # d轴电流环带宽
    PARAM_DCLKP = 0x0E  # d轴电流环Kp
    PARAM_DCLKI = 0x0F  # d轴电流环Ki
    PARAM_QCLBW = 0x10  # q轴电流环带宽
    PARAM_QCLKP = 0x11  # q轴电流环Kp
    PARAM_QCLKI = 0x12  # q轴电流环Ki
    PARAM_CFALPHA = 0x13  # 电流滤波系数
    PARAM_TUNE_CURRENT = 0x14  # 校准电流
    PARAM_TUNE_VEL = 0x15  # 校准速度
    PARAM_ROTOR_OBS = 0x16  # 转子观测器
    PARAM_ABS_POS_OBS = 0x17  # 绝对位置观测器
    PARAM_THETA_OFFSET = 0x18  # 角度偏移
    PARAM_POSITIVE_DIR = 0x19  # 方向反转
    PARAM_THETA_SOURCE = 0x1A  # 角度源
    PARAM_VEL_SOURCE = 0x1B  # 速度源
    PARAM_HFI_SMO_THRESHOLD = 0x1C  # HFI/SMO切换阈值
    PARAM_HALL_TABLE_0 = 0x1D  # HALL编码器表0
    PARAM_HALL_TABLE_1 = 0x1E  # HALL编码器表1
    PARAM_HALL_TABLE_2 = 0x1F  # HALL编码器表2
    PARAM_HALL_TABLE_3 = 0x20  # HALL编码器表3
    PARAM_HALL_TABLE_4 = 0x21  # HALL编码器表4
    PARAM_HALL_TABLE_5 = 0x22  # HALL编码器表5
    PARAM_HALL_TABLE_6 = 0x23  # HALL编码器表6
    PARAM_HALL_TABLE_7 = 0x24  # HALL编码器表7
    PARAM_ABI_COUNT = 0x25  # ABI编码器线数
    PARAM_SIN_AMP = 0x26  # SIN幅值
    PARAM_COS_AMP = 0x27  # COS幅值
    PARAM_SIN_OFFSET = 0x28  # SIN偏移
    PARAM_COS_OFFSET = 0x29  # COS偏移
    PARAM_SINCOS_FL = 0x2A  # SINCOS滤波系数
    PARAM_ENC_CHIP = 0x2B  # 编码器芯片
    PARAM_VLKP = 0x2C  # 速度环Kp
    PARAM_VLKI = 0x2D  # 速度环Ki
    PARAM_POS_PIDMODE = 0x2E  # 位置环模式
    PARAM_PLKP = 0x2F  # 位置环Kp
    PARAM_PLKI = 0x30  # 位置环Ki
    PARAM_PLKD = 0x31  # 位置环Kd
    PARAM_PLALPHA = 0x32  # 位置环滤波系数
    PARAM_MIT_KP = 0x33  # MIT 刚度(Kp)
    PARAM_MIT_KD = 0x34  # MIT 阻尼(Kd)
    PARAM_MIT_KPMAX = 0x35  # MIT 最大刚度
    PARAM_MIT_KDMAX = 0x36  # MIT 最大阻尼
    PARAM_MIT_TMAX = 0x37  # MIT最大扭矩
    PARAM_FWLKP = 0x38  # 弱磁Kp
    PARAM_FWLKI = 0x39  # 弱磁Ki
    PARAM_TRAJ_TYPE = 0x3A  # 轨迹类型
    PARAM_TRAJ_LIMIT_D1 = 0x3B  # 轨迹一阶限幅
    PARAM_TRAJ_LIMIT_D2 = 0x3C  # 轨迹二阶限幅
    PARAM_TRAJ_LIMIT_D3 = 0x3D  # 轨迹三阶限幅
    PARAM_TRAJ_TOLERANCE = 0x3E  # 轨迹容差
    PARAM_UNDERVOLTAGE_THRESHOLD = 0x3F  # 欠压阈值
    PARAM_OVERVOLTAGE_THRESHOLD = 0x40  # 过压阈值
    PARAM_CURRENT_THRESHOLD = 0x41  # 过流阈值
    PARAM_ELEC_TOLERANCE = 0x42  # 电气超限容忍度
    PARAM_ELEC_TOLERANCE_TIME_MS = 0x43  # 电气超限容忍时间
    PARAM_VEL_LIMIT = 0x44  # 速度限制
    PARAM_POS_MIN_LIMIT = 0x45  # 最小位置
    PARAM_POS_MAX_LIMIT = 0x46  # 最大位置
    PARAM_TAU_LIMIT = 0x47  # 力矩限制
    PARAM_MOTION_TOLERANCE = 0x48  # 运动超限容忍度
    PARAM_MOTION_TOLERANCE_TIME_MS = 0x49  # 运动超限容忍时间
    PARAM_CAN_RATE = 0x4A  # CAN速率
    PARAM_CAN_ID = 0x4B  # CAN ID
    PARAM_CAN_MODE = 0x4C  # CAN模式
    NUM_OF_PARAM = 0x4D  # 参数总数
PARAM_NUM = 0x4D

class Didx:
    DATA_CURRENT_U = 0x01  # U相电流
    DATA_CURRENT_V = 0x02  # V相电流
    DATA_CURRENT_W = 0x03  # W相电流
    DATA_VOLTAGE_Q = 0x04  # Q轴电压
    DATA_VOLTAGE_D = 0x05  # D轴电压
    DATA_CURRENT_ALPHA = 0x06  # α轴电流
    DATA_CURRENT_BETA = 0x07  # β轴电流
    DATA_CURRENT_Q = 0x08  # Q轴电流
    DATA_CURRENT_D = 0x09  # D轴电流
    DATA_CURRENT_Q_REF = 0x0A  # Q轴电流参考
    DATA_CURRENT_D_REF = 0x0B  # D轴电流参考
    DATA_VELOCITY = 0x0C  # 速度
    DATA_VELOCITY_REF = 0x0D  # 速度参考
    DATA_THETA_ELEC = 0x0E  # 电角度
    DATA_THETA_MECH = 0x0F  # 机械角度
    DATA_POSITION = 0x10  # 位置
    DATA_POSITION_REF = 0x11  # 位置参考
    DATA_MINUTES = 0x12  # 分钟
    DATA_SECONDS = 0x13  # 秒
    DATA_STATE = 0x14  # 状态
    DATA_FAULT = 0x15  # 故障
    DATA_WARNING = 0x16  # 警告
    DATA_VBUS = 0x17  # 母线电压
    DATA_DRIVER_TEMP = 0x18  # 驱动温度
    DATA_MOTOR_TEMP = 0x19  # 电机温度
    NUM_OF_DATA = 0x1A  # 观测字段总数
DATA_NUM = 0x1A

# CMD ID
class eCmdId:
    CMD_CONNECT = 0xF0  # 连接状态 1/0
    CMD_HEARTBEAT = 0xFB  # 心跳/数据流请求
    CMD_CORE_NRST = 0xFD  # 核心复位
    CMD_SYSTEM_RESET = 0xFC  # 系统复位
    CMD_RL_TUNNING = 0xE1  # RL校准
    CMD_PSIF_TUNNING = 0xE2  # Psif校准
    CMD_MECH_TUNNING = 0xE3  # 机械参数校准
    CMD_OBS_TUNNING = 0xE4  # 观测器校准
    CMD_PID_TUNNING = 0xE5  # PID参数校准
    CMD_MIT_TUNNING = 0xE6  # MIT参数校准
    CMD_PARAM_ALLRELOAD = 0xC1  # 参数一键重置
    CMD_PARAM_ALLWRITE = 0xC2  # 参数一键写入
    CMD_PARAM_ALLREAD = 0xC3  # 参数一键读取
    CMD_PARAM_RELOAD = 0xC4  # 参数重置
    CMD_PARAM_WRITE = 0xC5  # 参数写入
    CMD_PARAM_READ = 0xC6  # 参数读取
    CMD_PARAM_SAVE = 0xC7  # 参数保存
    CMD_DATA_ASK = 0xC8  # 数据查询
    CMD_DATA_STREAM_SET = 0xC9  # 数据流设置
    CMD_OPENLOOP_THETA = 0xB1  # 开环角度控制
    CMD_OPENLOOP_VEL = 0xB2  # 开环速度控制
    CMD_CUR_TAU_MODE = 0xB3  # 电流/力矩控制
    CMD_PID_VEL = 0xB4  # 速度PID控制
    CMD_PID_POS = 0xB5  # 位置PID控制
    CMD_MIT_POS = 0xB6  # MIT控制
    CMD_MIT_CONFIG = 0xB7  # MITKpKd配置
    CMD_MIT_VT = 0xB8  # MIT控制补充VT
    CMD_DISABLE = 0xBB  # 释放电机
    CMD_SET_ZERO_POS = 0xBD  # 设置零点
    CMD_SET_LIMIT_POS = 0xBE  # 设置极限位置
    CMD_IAP_ENTER = 0xA1  # 进入IAP模式
    CMD_IAP_ERASE_FLASH = 0xA2  # 擦除Flash
    CMD_IAP_WRITE_FLASH = 0xA3  # 写入Flash
    CMD_IAP_VERIFY_FLASH = 0xA4  # 校验Flash
    CMD_IAP_EXIT = 0xA5  # 退出IAP模式
    CMD_ASK_BL_VERSION = 0xA6  # 查询bootload版本
    CMD_ASK_APP_VERSION = 0xA7  # 查询app版本
    CMD_ASK_DEVICE_MESSAGE = 0xA8  # 查询设备信息

CmdId = eCmdId

# 反馈 ID
class eFbId:
    FB_EXECUTE = 0x90  # 成功
    FB_FAILURE = 0x9E  # 失败

FbId = eFbId

# USBcdc/UART 协议格式
class eUartFrame:
    UART_PACKET_HEAD = 0x81  # 包头
    UART_PACKET_TAIL = 0x8E  # 包尾

UartFrame = eUartFrame

# 控制目标
class eCmdTarget:
    TARGET_THETA_E = 0x71  # 电角度
    TARGET_VEL_E = 0x72  # 电角速度
    TARGET_DUTY = 0x73  # 占空比
    TARGET_CUR_Q = 0x74  # 拖动电流
    TARGET_TAU = 0x75  # 转矩
    TARGET_VEL = 0x76  # 速度
    TARGET_POS = 0x77  # 位置
    TARGET_PVT_TP = 0x78  # PVT时间点
    TARGET_MIT_TAU_FF = 0x78  # MIT前馈力矩

CmdTarget = eCmdTarget

# 命令 / 反馈 / 帧格式 / 控制目标元数据
COMMANDS = {
    'CMD_CONNECT': {"id": 0xF0, "comment": '连接状态 1/0'},
    'CMD_HEARTBEAT': {"id": 0xFB, "comment": '心跳/数据流请求'},
    'CMD_CORE_NRST': {"id": 0xFD, "comment": '核心复位'},
    'CMD_SYSTEM_RESET': {"id": 0xFC, "comment": '系统复位'},
    'CMD_RL_TUNNING': {"id": 0xE1, "comment": 'RL校准'},
    'CMD_PSIF_TUNNING': {"id": 0xE2, "comment": 'Psif校准'},
    'CMD_MECH_TUNNING': {"id": 0xE3, "comment": '机械参数校准'},
    'CMD_OBS_TUNNING': {"id": 0xE4, "comment": '观测器校准'},
    'CMD_PID_TUNNING': {"id": 0xE5, "comment": 'PID参数校准'},
    'CMD_MIT_TUNNING': {"id": 0xE6, "comment": 'MIT参数校准'},
    'CMD_PARAM_ALLRELOAD': {"id": 0xC1, "comment": '参数一键重置'},
    'CMD_PARAM_ALLWRITE': {"id": 0xC2, "comment": '参数一键写入'},
    'CMD_PARAM_ALLREAD': {"id": 0xC3, "comment": '参数一键读取'},
    'CMD_PARAM_RELOAD': {"id": 0xC4, "comment": '参数重置'},
    'CMD_PARAM_WRITE': {"id": 0xC5, "comment": '参数写入'},
    'CMD_PARAM_READ': {"id": 0xC6, "comment": '参数读取'},
    'CMD_PARAM_SAVE': {"id": 0xC7, "comment": '参数保存'},
    'CMD_DATA_ASK': {"id": 0xC8, "comment": '数据查询'},
    'CMD_DATA_STREAM_SET': {"id": 0xC9, "comment": '数据流设置'},
    'CMD_OPENLOOP_THETA': {"id": 0xB1, "comment": '开环角度控制'},
    'CMD_OPENLOOP_VEL': {"id": 0xB2, "comment": '开环速度控制'},
    'CMD_CUR_TAU_MODE': {"id": 0xB3, "comment": '电流/力矩控制'},
    'CMD_PID_VEL': {"id": 0xB4, "comment": '速度PID控制'},
    'CMD_PID_POS': {"id": 0xB5, "comment": '位置PID控制'},
    'CMD_MIT_POS': {"id": 0xB6, "comment": 'MIT控制'},
    'CMD_MIT_CONFIG': {"id": 0xB7, "comment": 'MITKpKd配置'},
    'CMD_MIT_VT': {"id": 0xB8, "comment": 'MIT控制补充VT'},
    'CMD_DISABLE': {"id": 0xBB, "comment": '释放电机'},
    'CMD_SET_ZERO_POS': {"id": 0xBD, "comment": '设置零点'},
    'CMD_SET_LIMIT_POS': {"id": 0xBE, "comment": '设置极限位置'},
    'CMD_IAP_ENTER': {"id": 0xA1, "comment": '进入IAP模式'},
    'CMD_IAP_ERASE_FLASH': {"id": 0xA2, "comment": '擦除Flash'},
    'CMD_IAP_WRITE_FLASH': {"id": 0xA3, "comment": '写入Flash'},
    'CMD_IAP_VERIFY_FLASH': {"id": 0xA4, "comment": '校验Flash'},
    'CMD_IAP_EXIT': {"id": 0xA5, "comment": '退出IAP模式'},
    'CMD_ASK_BL_VERSION': {"id": 0xA6, "comment": '查询bootload版本'},
    'CMD_ASK_APP_VERSION': {"id": 0xA7, "comment": '查询app版本'},
    'CMD_ASK_DEVICE_MESSAGE': {"id": 0xA8, "comment": '查询设备信息'},
}

FEEDBACKS = {
    'FB_EXECUTE': {"id": 0x90, "comment": '成功'},
    'FB_FAILURE': {"id": 0x9E, "comment": '失败'},
}

FRAME = {
    'UART_PACKET_HEAD': {"id": 0x81, "comment": '包头'},
    'UART_PACKET_TAIL': {"id": 0x8E, "comment": '包尾'},
}

CMD_TARGETS = {
    'TARGET_THETA_E': {"id": 0x71, "comment": '电角度'},
    'TARGET_VEL_E': {"id": 0x72, "comment": '电角速度'},
    'TARGET_DUTY': {"id": 0x73, "comment": '占空比'},
    'TARGET_CUR_Q': {"id": 0x74, "comment": '拖动电流'},
    'TARGET_TAU': {"id": 0x75, "comment": '转矩'},
    'TARGET_VEL': {"id": 0x76, "comment": '速度'},
    'TARGET_POS': {"id": 0x77, "comment": '位置'},
    'TARGET_PVT_TP': {"id": 0x78, "comment": 'PVT时间点'},
    'TARGET_MIT_TAU_FF': {"id": 0x78, "comment": 'MIT前馈力矩'},
}

# 电机类型
class eMotorList:
    MOTOR_PMSM = 0  # PMSM-永磁同步电机
    MOTOR_BLDC = 1  # BLDC-无刷直流电机

MotorList = eMotorList

# 观测器
class eObsList:
    OBS_NONE = 0  # 无观测
    OBS_INT_ENC = 1  # 内置编码器
    OBS_ENC_HALL = 2  # 霍尔编码器
    OBS_ENC_SPI = 3  # SPI编码器
    OBS_ENC_ABI = 4  # ABI编码器
    OBS_ENC_SINCOS = 5  # 正余弦编码器
    OBS_HFI_SMO = 6  # HFI+SMO观测器

ObsList = eObsList

# 角度源
class eThetaSource:
    THETA_PLL = 0  # PLL
    THETA_RAW = 1  # 原始角度

ThetaSource = eThetaSource

# 速度源
class eVelSource:
    VEL_PLL = 0  # PLL
    VEL_FDM = 1  # 差分速度

VelSource = eVelSource

# spi编码器芯片
class eSpiEncList:
    ENC_MT6816 = 0  # MT6816
    ENC_MT6835 = 1  # MT6835
    ENC_AS5047 = 2  # AS5047

SpiEncList = eSpiEncList

# 位置环pid模式
class ePosPidmode:
    POS_PID_PVC = 0  # PVC三环级联(抗干扰)
    POS_PID_PC = 1  # PC双环(高响应)

PosPidmode = ePosPidmode

# 轨迹规划模式
class eTrajMode:
    TRAJ_DISABLE = 0  # 禁用
    TRAJ_TRAPEZOID = 1  # 梯形轨迹(斜坡缓冲)
    TRAJ_S_CURVE = 2  # 曲线轨迹(双重缓冲)

TrajMode = eTrajMode

# CAN速率
class eCanRate:
    CAN_RATE_1000K = 0  # 1Mbps
    CAN_RATE_500K = 1  # 500Kbps

CanRate = eCanRate

# CAN模式
class eCanMode:
    CAN_SILENCE = 0  # 静默模式
    CAN_ANSWER = 1  # 响应模式

CanMode = eCanMode

# 核心状态
class eCoreState:
    CORE_INIT = 0  # INIT
    CORE_IDLE = 1  # IDLE
    CORE_TUNING = 2  # TUNING
    CORE_FAULT = 3  # FAULT
    CORE_WARNING = 4  # WARNING
    CORE_OPENLOOP = 5  # 开环模式
    CORE_CURRENT_MODE = 6  # 电流模式
    CORE_PID_SPEED = 7  # PID速度模式
    CORE_PID_POSITION = 8  # PID位置模式
    CORE_MIT_MODE = 9  # MIT模式

CoreState = eCoreState

# 错误
class eFault:
    FAULT_NONE = 0  # NONE
    FAULT_OBS_INIT = 1  # 观测器初始化失败
    FAULT_BUSCOM_INIT = 2  # 总线通信初始化失败
    FAULT_RS_TUNE = 3  # 电阻整定失败
    FAULT_RS_IMBALANCE = 4  # 相电阻不平衡
    FAULT_LS_TUNE = 5  # 电感整定失败
    FAULT_PSIF_TUNE = 6  # 磁链整定失败
    FAULT_OBS_TUNE = 7  # 观测器校准失败
    FAULT_POLE_PAIR_MISMATCH = 8  # 极对数不匹配
    FAULT_MOTOR_LOCK = 9  # 电机堵转
    FAULT_MECH_PARAM_FAIL = 10  # 机械参数校准失败
    FAULT_OVERVOLTAGE = 11  # 过电压
    FAULT_UNDERVOLTAGE = 12  # 低电压
    FAULT_OVERCURRENT = 13  # 过电流

Fault = eFault

# 警告
class eWarning:
    WARNING_NONE = 0  # NONE
    WARNING_OVERTEMP = 1  # 过温
    WARNING_OVERSPEED = 2  # 超速
    WARNING_POSITION_LIMIT = 3  # 位置超限
    WARNING_BUSCOM_RUN = 4  # 总线通讯异常
    WARNING_OBS_RUN = 5  # 观测器运行异常

Warning = eWarning

class ProtocolField:
    __slots__ = ("id", "id_name", "field", "name", "unit", "val_type", "default", "options")

    def __init__(self, id, id_name, field, name, unit, val_type, default=0, options=None):
        self.id = id
        self.id_name = id_name
        self.field = field
        self.name = name
        self.unit = unit
        self.val_type = val_type
        self.default = default
        self.options = options

    def __repr__(self):
        return f"ProtocolField({self.id_name})"

PARAMETERS = {
    Pidx.PARAM_MOTOR_TYPE: ProtocolField(Pidx.PARAM_MOTOR_TYPE, 'PARAM_MOTOR_TYPE', 'motor_type', '电机类型', '', 'FLD_U8', 0, [('PMSM-永磁同步电机', 0), ('BLDC-无刷直流电机', 1)]),
    Pidx.PARAM_MOTOR_POLEPAIRS: ProtocolField(Pidx.PARAM_MOTOR_POLEPAIRS, 'PARAM_MOTOR_POLEPAIRS', 'motor_polepairs', '极对数', '', 'FLD_U8', 7, None),
    Pidx.PARAM_MOTOR_KV: ProtocolField(Pidx.PARAM_MOTOR_KV, 'PARAM_MOTOR_KV', 'motor_kv', '电机KV值', 'rpm/V', 'FLD_F32', 550, None),
    Pidx.PARAM_MOTOR_RATED_CURRENT: ProtocolField(Pidx.PARAM_MOTOR_RATED_CURRENT, 'PARAM_MOTOR_RATED_CURRENT', 'motor_rated_current', '额定电流', 'A', 'FLD_F32', 3, None),
    Pidx.PARAM_MOTOR_RS: ProtocolField(Pidx.PARAM_MOTOR_RS, 'PARAM_MOTOR_RS', 'motor_rs', '定子电阻(Rs)', 'Ω', 'FLD_F32', 0.5, None),
    Pidx.PARAM_MOTOR_LD: ProtocolField(Pidx.PARAM_MOTOR_LD, 'PARAM_MOTOR_LD', 'motor_ld', 'd轴电感(Ld)', 'H', 'FLD_F32', 4e-06, None),
    Pidx.PARAM_MOTOR_LQ: ProtocolField(Pidx.PARAM_MOTOR_LQ, 'PARAM_MOTOR_LQ', 'motor_lq', 'q轴电感(Lq)', 'H', 'FLD_F32', 4e-06, None),
    Pidx.PARAM_MOTOR_PSIF: ProtocolField(Pidx.PARAM_MOTOR_PSIF, 'PARAM_MOTOR_PSIF', 'motor_psif', '永磁体磁链(λ)', 'Wb', 'FLD_F32', 0.0, None),
    Pidx.PARAM_MOTOR_KE: ProtocolField(Pidx.PARAM_MOTOR_KE, 'PARAM_MOTOR_KE', 'motor_ke', '反电动势常数(Ke)', 'V/(rad/s)', 'FLD_F32', 0.0, None),
    Pidx.PARAM_MOTOR_J: ProtocolField(Pidx.PARAM_MOTOR_J, 'PARAM_MOTOR_J', 'motor_j', '转动惯量(J)', 'kg·m²', 'FLD_F32', 0.0, None),
    Pidx.PARAM_MOTOR_B: ProtocolField(Pidx.PARAM_MOTOR_B, 'PARAM_MOTOR_B', 'motor_b', '粘滞摩擦系数(B)', 'N·m·s/rad', 'FLD_F32', 0.0, None),
    Pidx.PARAM_CLBW_COEF: ProtocolField(Pidx.PARAM_CLBW_COEF, 'PARAM_CLBW_COEF', 'clbw_coef', '电流环带宽系数', '0~1', 'FLD_F32', 0.5, None),
    Pidx.PARAM_DCLBW: ProtocolField(Pidx.PARAM_DCLBW, 'PARAM_DCLBW', 'dclbw', 'd轴电流环带宽', 'rad/s', 'FLD_F32', 0.0, None),
    Pidx.PARAM_DCLKP: ProtocolField(Pidx.PARAM_DCLKP, 'PARAM_DCLKP', 'dclkp', 'd轴电流环Kp', 'A/V', 'FLD_F32', 0.0, None),
    Pidx.PARAM_DCLKI: ProtocolField(Pidx.PARAM_DCLKI, 'PARAM_DCLKI', 'dclki', 'd轴电流环Ki', 'A/(V·s)', 'FLD_F32', 0.0, None),
    Pidx.PARAM_QCLBW: ProtocolField(Pidx.PARAM_QCLBW, 'PARAM_QCLBW', 'qclbw', 'q轴电流环带宽', 'rad/s', 'FLD_F32', 0.0, None),
    Pidx.PARAM_QCLKP: ProtocolField(Pidx.PARAM_QCLKP, 'PARAM_QCLKP', 'qclkp', 'q轴电流环Kp', 'A/V', 'FLD_F32', 0.0, None),
    Pidx.PARAM_QCLKI: ProtocolField(Pidx.PARAM_QCLKI, 'PARAM_QCLKI', 'qclki', 'q轴电流环Ki', 'A/(V·s)', 'FLD_F32', 0.0, None),
    Pidx.PARAM_CFALPHA: ProtocolField(Pidx.PARAM_CFALPHA, 'PARAM_CFALPHA', 'cfalpha', '电流滤波系数', '0~1', 'FLD_F32', 0.5, None),
    Pidx.PARAM_TUNE_CURRENT: ProtocolField(Pidx.PARAM_TUNE_CURRENT, 'PARAM_TUNE_CURRENT', 'tune_current', '校准电流', 'A', 'FLD_F32', 10, None),
    Pidx.PARAM_TUNE_VEL: ProtocolField(Pidx.PARAM_TUNE_VEL, 'PARAM_TUNE_VEL', 'tune_vel', '校准速度', 'rad/s', 'FLD_F32', 20, None),
    Pidx.PARAM_ROTOR_OBS: ProtocolField(Pidx.PARAM_ROTOR_OBS, 'PARAM_ROTOR_OBS', 'rotor_obs', '转子观测器', '', 'FLD_U8', 0, [('无观测', 0), ('内置编码器', 1), ('霍尔编码器', 2), ('SPI编码器', 3), ('ABI编码器', 4), ('正余弦编码器', 5), ('HFI+SMO观测器', 6)]),
    Pidx.PARAM_ABS_POS_OBS: ProtocolField(Pidx.PARAM_ABS_POS_OBS, 'PARAM_ABS_POS_OBS', 'abs_pos_obs', '绝对位置观测器', '', 'FLD_U8', 0, [('无观测', 0), ('内置编码器', 1), ('霍尔编码器', 2), ('SPI编码器', 3), ('ABI编码器', 4), ('正余弦编码器', 5), ('HFI+SMO观测器', 6)]),
    Pidx.PARAM_THETA_OFFSET: ProtocolField(Pidx.PARAM_THETA_OFFSET, 'PARAM_THETA_OFFSET', 'theta_offset', '角度偏移', 'rad', 'FLD_F32', 0.0, None),
    Pidx.PARAM_POSITIVE_DIR: ProtocolField(Pidx.PARAM_POSITIVE_DIR, 'PARAM_POSITIVE_DIR', 'positive_dir', '方向反转', '', 'FLD_BOOL', False, None),
    Pidx.PARAM_THETA_SOURCE: ProtocolField(Pidx.PARAM_THETA_SOURCE, 'PARAM_THETA_SOURCE', 'theta_source', '角度源', '', 'FLD_U8', 0, [('PLL', 0), ('原始角度', 1)]),
    Pidx.PARAM_VEL_SOURCE: ProtocolField(Pidx.PARAM_VEL_SOURCE, 'PARAM_VEL_SOURCE', 'vel_source', '速度源', '', 'FLD_U8', 0, [('PLL', 0), ('差分速度', 1)]),
    Pidx.PARAM_HFI_SMO_THRESHOLD: ProtocolField(Pidx.PARAM_HFI_SMO_THRESHOLD, 'PARAM_HFI_SMO_THRESHOLD', 'hfi_smo_threshold', 'HFI/SMO切换阈值', 'rad/s', 'FLD_F32', 0.0, None),
    Pidx.PARAM_HALL_TABLE_0: ProtocolField(Pidx.PARAM_HALL_TABLE_0, 'PARAM_HALL_TABLE_0', 'hall_table_0', 'HALL编码器表0', '', 'FLD_U8', 255, None),
    Pidx.PARAM_HALL_TABLE_1: ProtocolField(Pidx.PARAM_HALL_TABLE_1, 'PARAM_HALL_TABLE_1', 'hall_table_1', 'HALL编码器表1', '', 'FLD_U8', 255, None),
    Pidx.PARAM_HALL_TABLE_2: ProtocolField(Pidx.PARAM_HALL_TABLE_2, 'PARAM_HALL_TABLE_2', 'hall_table_2', 'HALL编码器表2', '', 'FLD_U8', 255, None),
    Pidx.PARAM_HALL_TABLE_3: ProtocolField(Pidx.PARAM_HALL_TABLE_3, 'PARAM_HALL_TABLE_3', 'hall_table_3', 'HALL编码器表3', '', 'FLD_U8', 255, None),
    Pidx.PARAM_HALL_TABLE_4: ProtocolField(Pidx.PARAM_HALL_TABLE_4, 'PARAM_HALL_TABLE_4', 'hall_table_4', 'HALL编码器表4', '', 'FLD_U8', 255, None),
    Pidx.PARAM_HALL_TABLE_5: ProtocolField(Pidx.PARAM_HALL_TABLE_5, 'PARAM_HALL_TABLE_5', 'hall_table_5', 'HALL编码器表5', '', 'FLD_U8', 255, None),
    Pidx.PARAM_HALL_TABLE_6: ProtocolField(Pidx.PARAM_HALL_TABLE_6, 'PARAM_HALL_TABLE_6', 'hall_table_6', 'HALL编码器表6', '', 'FLD_U8', 255, None),
    Pidx.PARAM_HALL_TABLE_7: ProtocolField(Pidx.PARAM_HALL_TABLE_7, 'PARAM_HALL_TABLE_7', 'hall_table_7', 'HALL编码器表7', '', 'FLD_U8', 255, None),
    Pidx.PARAM_ABI_COUNT: ProtocolField(Pidx.PARAM_ABI_COUNT, 'PARAM_ABI_COUNT', 'abi_count', 'ABI编码器线数', 'counts/rev', 'FLD_U16', 8192, None),
    Pidx.PARAM_SIN_AMP: ProtocolField(Pidx.PARAM_SIN_AMP, 'PARAM_SIN_AMP', 'sin_amp', 'SIN幅值', 'V', 'FLD_F32', 1.0, None),
    Pidx.PARAM_COS_AMP: ProtocolField(Pidx.PARAM_COS_AMP, 'PARAM_COS_AMP', 'cos_amp', 'COS幅值', 'V', 'FLD_F32', 1.0, None),
    Pidx.PARAM_SIN_OFFSET: ProtocolField(Pidx.PARAM_SIN_OFFSET, 'PARAM_SIN_OFFSET', 'sin_offset', 'SIN偏移', 'V', 'FLD_F32', 1.65, None),
    Pidx.PARAM_COS_OFFSET: ProtocolField(Pidx.PARAM_COS_OFFSET, 'PARAM_COS_OFFSET', 'cos_offset', 'COS偏移', 'V', 'FLD_F32', 1.65, None),
    Pidx.PARAM_SINCOS_FL: ProtocolField(Pidx.PARAM_SINCOS_FL, 'PARAM_SINCOS_FL', 'sincos_fl', 'SINCOS滤波系数', '0~1', 'FLD_F32', 0.5, None),
    Pidx.PARAM_ENC_CHIP: ProtocolField(Pidx.PARAM_ENC_CHIP, 'PARAM_ENC_CHIP', 'enc_chip', '编码器芯片', '', 'FLD_U8', 0, [('MT6816', 0), ('MT6835', 1), ('AS5047', 2)]),
    Pidx.PARAM_VLKP: ProtocolField(Pidx.PARAM_VLKP, 'PARAM_VLKP', 'vlkp', '速度环Kp', '', 'FLD_F32', 0.0, None),
    Pidx.PARAM_VLKI: ProtocolField(Pidx.PARAM_VLKI, 'PARAM_VLKI', 'vlki', '速度环Ki', '', 'FLD_F32', 0.0, None),
    Pidx.PARAM_POS_PIDMODE: ProtocolField(Pidx.PARAM_POS_PIDMODE, 'PARAM_POS_PIDMODE', 'pos_pidmode', '位置环模式', '', 'FLD_U8', 0, [('PVC三环级联(抗干扰)', 0), ('PC双环(高响应)', 1)]),
    Pidx.PARAM_PLKP: ProtocolField(Pidx.PARAM_PLKP, 'PARAM_PLKP', 'plkp', '位置环Kp', '1/s', 'FLD_F32', 0.0, None),
    Pidx.PARAM_PLKI: ProtocolField(Pidx.PARAM_PLKI, 'PARAM_PLKI', 'plki', '位置环Ki', '1/s²', 'FLD_F32', 0.0, None),
    Pidx.PARAM_PLKD: ProtocolField(Pidx.PARAM_PLKD, 'PARAM_PLKD', 'plkd', '位置环Kd', '', 'FLD_F32', 0.0, None),
    Pidx.PARAM_PLALPHA: ProtocolField(Pidx.PARAM_PLALPHA, 'PARAM_PLALPHA', 'plalpha', '位置环滤波系数', '0~1', 'FLD_F32', 0.5, None),
    Pidx.PARAM_MIT_KP: ProtocolField(Pidx.PARAM_MIT_KP, 'PARAM_MIT_KP', 'mit_kp', 'MIT 刚度(Kp)', 'N·m/rad', 'FLD_F32', 0.0, None),
    Pidx.PARAM_MIT_KD: ProtocolField(Pidx.PARAM_MIT_KD, 'PARAM_MIT_KD', 'mit_kd', 'MIT 阻尼(Kd)', 'N·m·s/rad', 'FLD_F32', 0.0, None),
    Pidx.PARAM_MIT_KPMAX: ProtocolField(Pidx.PARAM_MIT_KPMAX, 'PARAM_MIT_KPMAX', 'mit_kpmax', 'MIT 最大刚度', 'N·m/rad', 'FLD_F32', 0.0, None),
    Pidx.PARAM_MIT_KDMAX: ProtocolField(Pidx.PARAM_MIT_KDMAX, 'PARAM_MIT_KDMAX', 'mit_kdmax', 'MIT 最大阻尼', 'N·m·s/rad', 'FLD_F32', 0.0, None),
    Pidx.PARAM_MIT_TMAX: ProtocolField(Pidx.PARAM_MIT_TMAX, 'PARAM_MIT_TMAX', 'mit_tmax', 'MIT最大扭矩', 'N·m', 'FLD_F32', 0.0, None),
    Pidx.PARAM_FWLKP: ProtocolField(Pidx.PARAM_FWLKP, 'PARAM_FWLKP', 'fwlkp', '弱磁Kp', '', 'FLD_F32', 0.0, None),
    Pidx.PARAM_FWLKI: ProtocolField(Pidx.PARAM_FWLKI, 'PARAM_FWLKI', 'fwlki', '弱磁Ki', '', 'FLD_F32', 0.0, None),
    Pidx.PARAM_TRAJ_TYPE: ProtocolField(Pidx.PARAM_TRAJ_TYPE, 'PARAM_TRAJ_TYPE', 'traj_type', '轨迹类型', '', 'FLD_U8', 0, [('禁用', 0), ('梯形轨迹(斜坡缓冲)', 1), ('曲线轨迹(双重缓冲)', 2)]),
    Pidx.PARAM_TRAJ_LIMIT_D1: ProtocolField(Pidx.PARAM_TRAJ_LIMIT_D1, 'PARAM_TRAJ_LIMIT_D1', 'traj_limit_d1', '轨迹一阶限幅', 'unit/s', 'FLD_F32', 0.0, None),
    Pidx.PARAM_TRAJ_LIMIT_D2: ProtocolField(Pidx.PARAM_TRAJ_LIMIT_D2, 'PARAM_TRAJ_LIMIT_D2', 'traj_limit_d2', '轨迹二阶限幅', 'unit/s²', 'FLD_F32', 0.0, None),
    Pidx.PARAM_TRAJ_LIMIT_D3: ProtocolField(Pidx.PARAM_TRAJ_LIMIT_D3, 'PARAM_TRAJ_LIMIT_D3', 'traj_limit_d3', '轨迹三阶限幅', 'unit/s³', 'FLD_F32', 0.0, None),
    Pidx.PARAM_TRAJ_TOLERANCE: ProtocolField(Pidx.PARAM_TRAJ_TOLERANCE, 'PARAM_TRAJ_TOLERANCE', 'traj_tolerance', '轨迹容差', '0~1', 'FLD_F32', 0.0, None),
    Pidx.PARAM_UNDERVOLTAGE_THRESHOLD: ProtocolField(Pidx.PARAM_UNDERVOLTAGE_THRESHOLD, 'PARAM_UNDERVOLTAGE_THRESHOLD', 'undervoltage_threshold', '欠压阈值', 'V', 'FLD_F32', 20, None),
    Pidx.PARAM_OVERVOLTAGE_THRESHOLD: ProtocolField(Pidx.PARAM_OVERVOLTAGE_THRESHOLD, 'PARAM_OVERVOLTAGE_THRESHOLD', 'overvoltage_threshold', '过压阈值', 'V', 'FLD_F32', 26, None),
    Pidx.PARAM_CURRENT_THRESHOLD: ProtocolField(Pidx.PARAM_CURRENT_THRESHOLD, 'PARAM_CURRENT_THRESHOLD', 'current_threshold', '过流阈值', 'A', 'FLD_F32', 30, None),
    Pidx.PARAM_ELEC_TOLERANCE: ProtocolField(Pidx.PARAM_ELEC_TOLERANCE, 'PARAM_ELEC_TOLERANCE', 'elec_tolerance', '电气超限容忍度', '0~1', 'FLD_F32', 0.1, None),
    Pidx.PARAM_ELEC_TOLERANCE_TIME_MS: ProtocolField(Pidx.PARAM_ELEC_TOLERANCE_TIME_MS, 'PARAM_ELEC_TOLERANCE_TIME_MS', 'elec_tolerance_time_ms', '电气超限容忍时间', 'ms', 'FLD_F32', 1, None),
    Pidx.PARAM_VEL_LIMIT: ProtocolField(Pidx.PARAM_VEL_LIMIT, 'PARAM_VEL_LIMIT', 'vel_limit', '速度限制', 'rad/s', 'FLD_F32', 500, None),
    Pidx.PARAM_POS_MIN_LIMIT: ProtocolField(Pidx.PARAM_POS_MIN_LIMIT, 'PARAM_POS_MIN_LIMIT', 'pos_min_limit', '最小位置', 'rad', 'FLD_F32', -15000, None),
    Pidx.PARAM_POS_MAX_LIMIT: ProtocolField(Pidx.PARAM_POS_MAX_LIMIT, 'PARAM_POS_MAX_LIMIT', 'pos_max_limit', '最大位置', 'rad', 'FLD_F32', 15000, None),
    Pidx.PARAM_TAU_LIMIT: ProtocolField(Pidx.PARAM_TAU_LIMIT, 'PARAM_TAU_LIMIT', 'tau_limit', '力矩限制', 'N·m', 'FLD_F32', 10, None),
    Pidx.PARAM_MOTION_TOLERANCE: ProtocolField(Pidx.PARAM_MOTION_TOLERANCE, 'PARAM_MOTION_TOLERANCE', 'motion_tolerance', '运动超限容忍度', '0~1', 'FLD_F32', 0.1, None),
    Pidx.PARAM_MOTION_TOLERANCE_TIME_MS: ProtocolField(Pidx.PARAM_MOTION_TOLERANCE_TIME_MS, 'PARAM_MOTION_TOLERANCE_TIME_MS', 'motion_tolerance_time_ms', '运动超限容忍时间', 'ms', 'FLD_F32', 10, None),
    Pidx.PARAM_CAN_RATE: ProtocolField(Pidx.PARAM_CAN_RATE, 'PARAM_CAN_RATE', 'can_rate', 'CAN速率', '', 'FLD_U8', 0, [('1Mbps', 0), ('500Kbps', 1)]),
    Pidx.PARAM_CAN_ID: ProtocolField(Pidx.PARAM_CAN_ID, 'PARAM_CAN_ID', 'can_id', 'CAN ID', '', 'FLD_U32', 1, None),
    Pidx.PARAM_CAN_MODE: ProtocolField(Pidx.PARAM_CAN_MODE, 'PARAM_CAN_MODE', 'can_mode', 'CAN模式', '', 'FLD_U8', 0, [('静默模式', 0), ('响应模式', 1)]),
}
PARAM_BY_NAME = {field.id_name: field for field in PARAMETERS.values()}

OBSDATA = {
    Didx.DATA_CURRENT_U: ProtocolField(Didx.DATA_CURRENT_U, 'DATA_CURRENT_U', 'current_u', 'U相电流', 'A', 'FLD_F32', 0.0, None),
    Didx.DATA_CURRENT_V: ProtocolField(Didx.DATA_CURRENT_V, 'DATA_CURRENT_V', 'current_v', 'V相电流', 'A', 'FLD_F32', 0.0, None),
    Didx.DATA_CURRENT_W: ProtocolField(Didx.DATA_CURRENT_W, 'DATA_CURRENT_W', 'current_w', 'W相电流', 'A', 'FLD_F32', 0.0, None),
    Didx.DATA_VOLTAGE_Q: ProtocolField(Didx.DATA_VOLTAGE_Q, 'DATA_VOLTAGE_Q', 'voltage_q', 'Q轴电压', 'V', 'FLD_F32', 0.0, None),
    Didx.DATA_VOLTAGE_D: ProtocolField(Didx.DATA_VOLTAGE_D, 'DATA_VOLTAGE_D', 'voltage_d', 'D轴电压', 'V', 'FLD_F32', 0.0, None),
    Didx.DATA_CURRENT_ALPHA: ProtocolField(Didx.DATA_CURRENT_ALPHA, 'DATA_CURRENT_ALPHA', 'current_alpha', 'α轴电流', 'A', 'FLD_F32', 0.0, None),
    Didx.DATA_CURRENT_BETA: ProtocolField(Didx.DATA_CURRENT_BETA, 'DATA_CURRENT_BETA', 'current_beta', 'β轴电流', 'A', 'FLD_F32', 0.0, None),
    Didx.DATA_CURRENT_Q: ProtocolField(Didx.DATA_CURRENT_Q, 'DATA_CURRENT_Q', 'current_q', 'Q轴电流', 'A', 'FLD_F32', 0.0, None),
    Didx.DATA_CURRENT_D: ProtocolField(Didx.DATA_CURRENT_D, 'DATA_CURRENT_D', 'current_d', 'D轴电流', 'A', 'FLD_F32', 0.0, None),
    Didx.DATA_CURRENT_Q_REF: ProtocolField(Didx.DATA_CURRENT_Q_REF, 'DATA_CURRENT_Q_REF', 'current_q_ref', 'Q轴电流参考', 'A', 'FLD_F32', 0.0, None),
    Didx.DATA_CURRENT_D_REF: ProtocolField(Didx.DATA_CURRENT_D_REF, 'DATA_CURRENT_D_REF', 'current_d_ref', 'D轴电流参考', 'A', 'FLD_F32', 0.0, None),
    Didx.DATA_VELOCITY: ProtocolField(Didx.DATA_VELOCITY, 'DATA_VELOCITY', 'velocity', '速度', 'rad/s', 'FLD_F32', 0.0, None),
    Didx.DATA_VELOCITY_REF: ProtocolField(Didx.DATA_VELOCITY_REF, 'DATA_VELOCITY_REF', 'velocity_ref', '速度参考', 'rad/s', 'FLD_F32', 0.0, None),
    Didx.DATA_THETA_ELEC: ProtocolField(Didx.DATA_THETA_ELEC, 'DATA_THETA_ELEC', 'theta_elec', '电角度', 'rad', 'FLD_F32', 0.0, None),
    Didx.DATA_THETA_MECH: ProtocolField(Didx.DATA_THETA_MECH, 'DATA_THETA_MECH', 'theta_mech', '机械角度', 'rad', 'FLD_F32', 0.0, None),
    Didx.DATA_POSITION: ProtocolField(Didx.DATA_POSITION, 'DATA_POSITION', 'position', '位置', 'rad', 'FLD_F32', 0.0, None),
    Didx.DATA_POSITION_REF: ProtocolField(Didx.DATA_POSITION_REF, 'DATA_POSITION_REF', 'position_ref', '位置参考', 'rad', 'FLD_F32', 0.0, None),
    Didx.DATA_MINUTES: ProtocolField(Didx.DATA_MINUTES, 'DATA_MINUTES', 'minutes', '分钟', 'min', 'FLD_F32', 0.0, None),
    Didx.DATA_SECONDS: ProtocolField(Didx.DATA_SECONDS, 'DATA_SECONDS', 'seconds', '秒', 's', 'FLD_F32', 0.0, None),
    Didx.DATA_STATE: ProtocolField(Didx.DATA_STATE, 'DATA_STATE', 'state', '状态', '', 'FLD_U8', 0, [('INIT', 0), ('IDLE', 1), ('TUNING', 2), ('FAULT', 3), ('WARNING', 4), ('开环模式', 5), ('电流模式', 6), ('PID速度模式', 7), ('PID位置模式', 8), ('MIT模式', 9)]),
    Didx.DATA_FAULT: ProtocolField(Didx.DATA_FAULT, 'DATA_FAULT', 'fault', '故障', '', 'FLD_U8', 0, [('NONE', 0), ('观测器初始化失败', 1), ('总线通信初始化失败', 2), ('电阻整定失败', 3), ('相电阻不平衡', 4), ('电感整定失败', 5), ('磁链整定失败', 6), ('观测器校准失败', 7), ('极对数不匹配', 8), ('电机堵转', 9), ('机械参数校准失败', 10), ('过电压', 11), ('低电压', 12), ('过电流', 13)]),
    Didx.DATA_WARNING: ProtocolField(Didx.DATA_WARNING, 'DATA_WARNING', 'warning', '警告', '', 'FLD_U8', 0, [('NONE', 0), ('过温', 1), ('超速', 2), ('位置超限', 3), ('总线通讯异常', 4), ('观测器运行异常', 5)]),
    Didx.DATA_VBUS: ProtocolField(Didx.DATA_VBUS, 'DATA_VBUS', 'vbus', '母线电压', 'V', 'FLD_F32', 0.0, None),
    Didx.DATA_DRIVER_TEMP: ProtocolField(Didx.DATA_DRIVER_TEMP, 'DATA_DRIVER_TEMP', 'driver_temp', '驱动温度', '°C', 'FLD_F32', 0.0, None),
    Didx.DATA_MOTOR_TEMP: ProtocolField(Didx.DATA_MOTOR_TEMP, 'DATA_MOTOR_TEMP', 'motor_temp', '电机温度', '°C', 'FLD_F32', 0.0, None),
}
OBS_BY_NAME = {field.id_name: field for field in OBSDATA.values()}
