# 此文件由 protocol/gen_protocol.py 自动生成，请勿手动修改

class eFieldValueType:
    FLD_U8 = 0  # uint8_t
    FLD_U16 = 1  # uint16_t
    FLD_U32 = 2  # uint32_t
    FLD_F32 = 3  # float
    FLD_BOOL = 4  # bool
    FLD_I4 = 5  # int8_t
    FLD_I16 = 6  # int16_t
    FLD_I24 = 7  # int32_t

FieldValueType = eFieldValueType

class Pidx:
    PARAM_MOTOR_TYPE = 1  # 电机类型
    PARAM_MOTOR_POLEPAIRS = 2  # 极对数
    PARAM_MOTOR_KV = 3  # 电机KV值
    PARAM_MOTOR_RATED_CURRENT = 4  # 额定电流
    PARAM_MOTOR_RS = 5  # 定子电阻(Rs)
    PARAM_MOTOR_LD = 6  # d轴电感(Ld)
    PARAM_MOTOR_LQ = 7  # q轴电感(Lq)
    PARAM_MOTOR_PSIF = 8  # 永磁体磁链(λ)
    PARAM_MOTOR_KE = 9  # 反电动势常数(Ke)
    PARAM_MOTOR_J = 10  # 转动惯量(J)
    PARAM_MOTOR_B = 11  # 粘滞摩擦系数(B)
    PARAM_CLBW_COEF = 12  # 电流环带宽系数
    PARAM_DCLBW = 13  # d轴电流环带宽
    PARAM_DCLKP = 14  # d轴电流环Kp
    PARAM_DCLKI = 15  # d轴电流环Ki
    PARAM_QCLBW = 16  # q轴电流环带宽
    PARAM_QCLKP = 17  # q轴电流环Kp
    PARAM_QCLKI = 18  # q轴电流环Ki
    PARAM_CFALPHA = 19  # 电流滤波系数
    PARAM_TUNE_CURRENT = 20  # 校准电流
    PARAM_TUNE_VEL = 21  # 校准速度
    PARAM_ROTOR_OBS = 22  # 转子观测器
    PARAM_ABS_POS_OBS = 23  # 绝对位置观测器
    PARAM_THETA_OFFSET = 24  # 角度偏移
    PARAM_POSITIVE_DIR = 25  # 方向反转
    PARAM_THETA_SOURCE = 26  # 角度源
    PARAM_VEL_SOURCE = 27  # 速度源
    PARAM_HFI_SMO_THRESHOLD = 28  # HFI/SMO切换阈值
    PARAM_HALL_TABLE_0 = 29  # HALL编码器表0
    PARAM_HALL_TABLE_1 = 30  # HALL编码器表1
    PARAM_HALL_TABLE_2 = 31  # HALL编码器表2
    PARAM_HALL_TABLE_3 = 32  # HALL编码器表3
    PARAM_HALL_TABLE_4 = 33  # HALL编码器表4
    PARAM_HALL_TABLE_5 = 34  # HALL编码器表5
    PARAM_HALL_TABLE_6 = 35  # HALL编码器表6
    PARAM_HALL_TABLE_7 = 36  # HALL编码器表7
    PARAM_ABI_COUNT = 37  # ABI编码器线数
    PARAM_SIN_AMP = 38  # SIN幅值
    PARAM_COS_AMP = 39  # COS幅值
    PARAM_SIN_OFFSET = 40  # SIN偏移
    PARAM_COS_OFFSET = 41  # COS偏移
    PARAM_SINCOS_FL = 42  # SINCOS滤波系数
    PARAM_ENC_CHIP = 43  # 编码器芯片
    PARAM_VLKP = 44  # 速度环Kp
    PARAM_VLKI = 45  # 速度环Ki
    PARAM_VELOCITY_LIMIT = 46  # 速度限制
    PARAM_POS_PIDMODE = 47  # 位置环模式
    PARAM_PLKP = 48  # 位置环Kp
    PARAM_PLKI = 49  # 位置环Ki
    PARAM_PLKD = 50  # 位置环Kd
    PARAM_PLALPHA = 51  # 位置环滤波系数
    PARAM_POS_LIMIT_MODE = 52  # 位置限制模式
    PARAM_POS_MIN_LIMIT = 53  # 最小位置
    PARAM_POS_MAX_LIMIT = 54  # 最大位置
    PARAM_MIT_KP = 55  # MIT 刚度(Kp)
    PARAM_MIT_KD = 56  # MIT 阻尼(Kd)
    PARAM_MIT_KPMAX = 57  # MIT 最大刚度
    PARAM_MIT_KDMAX = 58  # MIT 最大阻尼
    PARAM_MIT_TMAX = 59  # MIT最大扭矩
    PARAM_FWLKP = 60  # 弱磁Kp
    PARAM_FWLKI = 61  # 弱磁Ki
    PARAM_TRAJ_MODE = 62  # 轨迹规划器模式
    PARAM_TRAJ_LIMIT_D1 = 63  # 轨迹一阶限幅
    PARAM_TRAJ_LIMIT_D2 = 64  # 轨迹二阶限幅
    PARAM_TRAJ_LIMIT_D3 = 65  # 轨迹三阶限幅
    PARAM_TRAJ_TOLERANCE = 66  # 轨迹容差
    PARAM_UNDERVOLTAGE_THRESHOLD = 67  # 欠压阈值
    PARAM_OVERVOLTAGE_THRESHOLD = 68  # 过压阈值
    PARAM_OVERTEMP_THRESHOLD = 69  # 过温阈值
    PARAM_PRO_TOLERANCE = 70  # 保护超限容忍度
    PARAM_PRO_TOLERANCE_TIME_MS = 71  # 保护超限容忍时间
    PARAM_CAN_RATE = 72  # CAN速率
    PARAM_CAN_ID = 73  # CAN ID
    PARAM_CAN_MODE = 74  # CAN模式
    NUM_OF_PARAM = 75  # PARAM总数
PARAM_NUM = 75

class Didx:
    DATA_CURRENT_U = 1  # U相电流
    DATA_CURRENT_V = 2  # V相电流
    DATA_CURRENT_W = 3  # W相电流
    DATA_VOLTAGE_Q = 4  # Q轴电压
    DATA_VOLTAGE_D = 5  # D轴电压
    DATA_CURRENT_ALPHA = 6  # α轴电流
    DATA_CURRENT_BETA = 7  # β轴电流
    DATA_CURRENT_Q = 8  # Q轴电流
    DATA_CURRENT_D = 9  # D轴电流
    DATA_CURRENT_Q_REF = 10  # Q轴电流参考
    DATA_CURRENT_D_REF = 11  # D轴电流参考
    DATA_VELOCITY = 12  # 速度
    DATA_VELOCITY_REF = 13  # 速度参考
    DATA_THETA_ELEC = 14  # 电角度
    DATA_THETA_MECH = 15  # 机械角度
    DATA_POSITION = 16  # 位置
    DATA_POSITION_REF = 17  # 位置参考
    DATA_STATE = 18  # 状态
    DATA_FAULT = 19  # 故障
    DATA_WARNING = 20  # 警告
    DATA_VBUS = 21  # 母线电压
    DATA_DRIVER_TEMP = 22  # 驱动温度
    DATA_MOTOR_TEMP = 23  # 电机温度
    NUM_OF_DATA = 24  # DATA总数
DATA_NUM = 24

class CmdId:
    CMD_CONNECT = 240  # 连接状态
    CMD_HEARTBEAT = 251  # 心跳请求；只回状态包，不与示波数据流耦合
    CMD_CORE_NRST = 253  # 核心复位
    CMD_SYSTEM_RESET = 252  # 系统复位
    CMD_RL_TUNNING = 225  # RL校准
    CMD_PSIF_TUNNING = 226  # Psif校准
    CMD_MECH_TUNNING = 227  # 机械参数校准
    CMD_OBS_TUNNING = 228  # 观测器校准
    CMD_PID_TUNNING = 229  # PID参数校准
    CMD_MIT_TUNNING = 230  # MIT参数校准
    CMD_PARAM_RELOAD = 196  # 参数重置；0xFF 表示全部重置
    CMD_PARAM_WRITE = 197  # 参数写入；0xFF 表示应用已写入参数
    CMD_PARAM_READ = 198  # 参数读取
    CMD_PARAM_SAVE = 199  # 参数保存
    CMD_DATA_ASK = 200  # 观测数据查询；回复解码需要请求的 id 上下文
    CMD_DATA_STREAM_SET = 201  # 示波数据流订阅，最多 5 路
    CMD_OPENLOOP_CTRL = 177  # 开环控制
    CMD_CUR_TAU_CTRL = 179  # 电流/力矩控制
    CMD_PID_VEL_CTRL = 180  # 速度PID控制
    CMD_PID_POS_CTRL = 181  # 位置PID控制
    CMD_MIT_CTRL = 182  # MIT控制；配置正指数 6/8，线上为 -6/-8
    CMD_MIT_CONFIG = 183  # MIT Kp/Kd配置
    CMD_DISABLE = 187  # 释放电机
    CMD_SET_ZERO_POS = 189  # 设置零点
    CMD_SET_LIMIT_POS = 190  # 以当前位置设置极限位置
    CMD_IAP_ENTER = 161  # 进入IAP模式
    CMD_IAP_ERASE_FLASH = 162  # 擦除Flash
    CMD_IAP_WRITE_FLASH = 163  # 写入Flash
    CMD_IAP_VERIFY_FLASH = 164  # 校验Flash
    CMD_IAP_EXIT = 165  # 退出IAP模式
    CMD_ASK_BL_VERSION = 166  # 查询bootloader版本
    CMD_ASK_APP_VERSION = 167  # 查询app版本
    CMD_ASK_DEVICE_MESSAGE = 168  # 查询设备信息
    CMD_DATA_STREAM = 202  # 示波数据流主动上报；由订阅决定内容

class eUartFrame:
    UART_PACKET_HEAD = 129  # 包头
    UART_PACKET_TAIL = 142  # 包尾

UartFrame = eUartFrame

class eFeedBack:
    FB_EXECUTE = 145  # 执行成功
    FB_FAILURE = 158  # 执行失败

FeedBack = eFeedBack

class eMotorList:
    MOTOR_PMSM = 0  # PMSM-永磁同步电机
    MOTOR_BLDC = 1  # BLDC-无刷直流电机

MotorList = eMotorList

class eObsList:
    OBS_NONE = 0  # 无观测
    OBS_INT_ENC = 1  # 内置编码器
    OBS_ENC_HALL = 2  # 霍尔编码器
    OBS_ENC_SPI = 3  # SPI编码器
    OBS_ENC_ABI = 4  # ABI编码器
    OBS_ENC_SINCOS = 5  # 正余弦编码器
    OBS_HFI_SMO = 6  # HFI+SMO观测器

ObsList = eObsList

class eThetaSource:
    THETA_PLL = 0  # PLL
    THETA_RAW = 1  # 原始角度

ThetaSource = eThetaSource

class eVelSource:
    VEL_PLL = 0  # PLL
    VEL_FDM = 1  # 差分速度

VelSource = eVelSource

class eSpiEncList:
    ENC_MT6816 = 0  # MT6816
    ENC_MT6835 = 1  # MT6835
    ENC_AS5047 = 2  # AS5047

SpiEncList = eSpiEncList

class ePosPidmode:
    POS_PID_PVC = 0  # PVC三环级联(抗干扰)
    POS_PID_PC = 1  # PC双环(高响应)

PosPidmode = ePosPidmode

class ePosLimitMode:
    POS_LIMIT_DISABLE = 0  # 禁用
    POS_LIMIT_MIN = 1  # 最小位置限制
    POS_LIMIT_MAX = 2  # 最大位置限制
    POS_LIMIT_BOTH = 3  # 双端位置限制

PosLimitMode = ePosLimitMode

class eTrajMode:
    TRAJ_DISABLE = 0  # 禁用
    TRAJ_TRAPEZOID = 1  # 梯形轨迹(斜坡缓冲)
    TRAJ_S_CURVE = 2  # 曲线轨迹(双重缓冲)

TrajMode = eTrajMode

class eCanRate:
    CAN_RATE_1000K = 0  # 1Mbps
    CAN_RATE_500K = 1  # 500Kbps

CanRate = eCanRate

class eCanMode:
    CAN_SILENCE = 0  # 静默模式
    CAN_ANSWER = 1  # 响应模式

CanMode = eCanMode

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

class eWarning:
    WARNING_NONE = 0  # NONE
    WARNING_OVERTEMP = 1  # 过温
    WARNING_OVERSPEED = 2  # 超速
    WARNING_POSITION_LIMIT = 3  # 位置超限
    WARNING_BUSCOM_RUN = 4  # 总线通讯异常
    WARNING_OBS_RUN = 5  # 观测器运行异常
    WARNING_INVALID_INPUT = 6  # 非法输入

Warning = eWarning

eCmdId = CmdId
FbId = FeedBack

class ProtocolField:
    __slots__ = ('id', 'id_name', 'field', 'name', 'unit', 'val_type', 'default', 'options')
    def __init__(self, id, id_name, field, name, unit, val_type, default=0, options=None):
        self.id, self.id_name, self.field = id, id_name, field
        self.name, self.unit, self.val_type = name, unit, val_type
        self.default, self.options = default, options
    def __repr__(self):
        return f'ProtocolField({self.id_name})'

PARAMETERS = {
    1: ProtocolField(1, 'PARAM_MOTOR_TYPE', 'motor_type', '电机类型', '', 'FLD_U8', 0, [('PMSM-永磁同步电机', 0), ('BLDC-无刷直流电机', 1)]),
    2: ProtocolField(2, 'PARAM_MOTOR_POLEPAIRS', 'motor_polepairs', '极对数', '', 'FLD_U8', 7, None),
    3: ProtocolField(3, 'PARAM_MOTOR_KV', 'motor_kv', '电机KV值', 'rpm/V', 'FLD_F32', 550, None),
    4: ProtocolField(4, 'PARAM_MOTOR_RATED_CURRENT', 'motor_rated_current', '额定电流', 'A', 'FLD_F32', 3, None),
    5: ProtocolField(5, 'PARAM_MOTOR_RS', 'motor_rs', '定子电阻(Rs)', 'Ω', 'FLD_F32', 0.5, None),
    6: ProtocolField(6, 'PARAM_MOTOR_LD', 'motor_ld', 'd轴电感(Ld)', 'H', 'FLD_F32', 4e-06, None),
    7: ProtocolField(7, 'PARAM_MOTOR_LQ', 'motor_lq', 'q轴电感(Lq)', 'H', 'FLD_F32', 4e-06, None),
    8: ProtocolField(8, 'PARAM_MOTOR_PSIF', 'motor_psif', '永磁体磁链(λ)', 'Wb', 'FLD_F32', 0.0, None),
    9: ProtocolField(9, 'PARAM_MOTOR_KE', 'motor_ke', '反电动势常数(Ke)', 'V/(rad/s)', 'FLD_F32', 0.0, None),
    10: ProtocolField(10, 'PARAM_MOTOR_J', 'motor_j', '转动惯量(J)', 'kg·m²', 'FLD_F32', 0.0, None),
    11: ProtocolField(11, 'PARAM_MOTOR_B', 'motor_b', '粘滞摩擦系数(B)', 'N·m·s/rad', 'FLD_F32', 0.0, None),
    12: ProtocolField(12, 'PARAM_CLBW_COEF', 'clbw_coef', '电流环带宽系数', '0~1', 'FLD_F32', 0.5, None),
    13: ProtocolField(13, 'PARAM_DCLBW', 'dclbw', 'd轴电流环带宽', 'rad/s', 'FLD_F32', 0.0, None),
    14: ProtocolField(14, 'PARAM_DCLKP', 'dclkp', 'd轴电流环Kp', 'A/V', 'FLD_F32', 0.0, None),
    15: ProtocolField(15, 'PARAM_DCLKI', 'dclki', 'd轴电流环Ki', 'A/(V·s)', 'FLD_F32', 0.0, None),
    16: ProtocolField(16, 'PARAM_QCLBW', 'qclbw', 'q轴电流环带宽', 'rad/s', 'FLD_F32', 0.0, None),
    17: ProtocolField(17, 'PARAM_QCLKP', 'qclkp', 'q轴电流环Kp', 'A/V', 'FLD_F32', 0.0, None),
    18: ProtocolField(18, 'PARAM_QCLKI', 'qclki', 'q轴电流环Ki', 'A/(V·s)', 'FLD_F32', 0.0, None),
    19: ProtocolField(19, 'PARAM_CFALPHA', 'cfalpha', '电流滤波系数', '0~1', 'FLD_F32', 0.5, None),
    20: ProtocolField(20, 'PARAM_TUNE_CURRENT', 'tune_current', '校准电流', 'A', 'FLD_F32', 10, None),
    21: ProtocolField(21, 'PARAM_TUNE_VEL', 'tune_vel', '校准速度', 'rad/s', 'FLD_F32', 20, None),
    22: ProtocolField(22, 'PARAM_ROTOR_OBS', 'rotor_obs', '转子观测器', '', 'FLD_U8', 0, [('无观测', 0), ('内置编码器', 1), ('霍尔编码器', 2), ('SPI编码器', 3), ('ABI编码器', 4), ('正余弦编码器', 5), ('HFI+SMO观测器', 6)]),
    23: ProtocolField(23, 'PARAM_ABS_POS_OBS', 'abs_pos_obs', '绝对位置观测器', '', 'FLD_U8', 0, [('无观测', 0), ('内置编码器', 1), ('霍尔编码器', 2), ('SPI编码器', 3), ('ABI编码器', 4), ('正余弦编码器', 5), ('HFI+SMO观测器', 6)]),
    24: ProtocolField(24, 'PARAM_THETA_OFFSET', 'theta_offset', '角度偏移', 'rad', 'FLD_F32', 0.0, None),
    25: ProtocolField(25, 'PARAM_POSITIVE_DIR', 'positive_dir', '方向反转', '', 'FLD_BOOL', False, None),
    26: ProtocolField(26, 'PARAM_THETA_SOURCE', 'theta_source', '角度源', '', 'FLD_U8', 0, [('PLL', 0), ('原始角度', 1)]),
    27: ProtocolField(27, 'PARAM_VEL_SOURCE', 'vel_source', '速度源', '', 'FLD_U8', 0, [('PLL', 0), ('差分速度', 1)]),
    28: ProtocolField(28, 'PARAM_HFI_SMO_THRESHOLD', 'hfi_smo_threshold', 'HFI/SMO切换阈值', 'rad/s', 'FLD_F32', 0.0, None),
    29: ProtocolField(29, 'PARAM_HALL_TABLE_0', 'hall_table_0', 'HALL编码器表0', '', 'FLD_U8', 255, None),
    30: ProtocolField(30, 'PARAM_HALL_TABLE_1', 'hall_table_1', 'HALL编码器表1', '', 'FLD_U8', 255, None),
    31: ProtocolField(31, 'PARAM_HALL_TABLE_2', 'hall_table_2', 'HALL编码器表2', '', 'FLD_U8', 255, None),
    32: ProtocolField(32, 'PARAM_HALL_TABLE_3', 'hall_table_3', 'HALL编码器表3', '', 'FLD_U8', 255, None),
    33: ProtocolField(33, 'PARAM_HALL_TABLE_4', 'hall_table_4', 'HALL编码器表4', '', 'FLD_U8', 255, None),
    34: ProtocolField(34, 'PARAM_HALL_TABLE_5', 'hall_table_5', 'HALL编码器表5', '', 'FLD_U8', 255, None),
    35: ProtocolField(35, 'PARAM_HALL_TABLE_6', 'hall_table_6', 'HALL编码器表6', '', 'FLD_U8', 255, None),
    36: ProtocolField(36, 'PARAM_HALL_TABLE_7', 'hall_table_7', 'HALL编码器表7', '', 'FLD_U8', 255, None),
    37: ProtocolField(37, 'PARAM_ABI_COUNT', 'abi_count', 'ABI编码器线数', 'counts/rev', 'FLD_U16', 8192, None),
    38: ProtocolField(38, 'PARAM_SIN_AMP', 'sin_amp', 'SIN幅值', 'V', 'FLD_F32', 1.0, None),
    39: ProtocolField(39, 'PARAM_COS_AMP', 'cos_amp', 'COS幅值', 'V', 'FLD_F32', 1.0, None),
    40: ProtocolField(40, 'PARAM_SIN_OFFSET', 'sin_offset', 'SIN偏移', 'V', 'FLD_F32', 1.65, None),
    41: ProtocolField(41, 'PARAM_COS_OFFSET', 'cos_offset', 'COS偏移', 'V', 'FLD_F32', 1.65, None),
    42: ProtocolField(42, 'PARAM_SINCOS_FL', 'sincos_fl', 'SINCOS滤波系数', '0~1', 'FLD_F32', 0.5, None),
    43: ProtocolField(43, 'PARAM_ENC_CHIP', 'enc_chip', '编码器芯片', '', 'FLD_U8', 0, [('MT6816', 0), ('MT6835', 1), ('AS5047', 2)]),
    44: ProtocolField(44, 'PARAM_VLKP', 'vlkp', '速度环Kp', '', 'FLD_F32', 0.0, None),
    45: ProtocolField(45, 'PARAM_VLKI', 'vlki', '速度环Ki', '', 'FLD_F32', 0.0, None),
    46: ProtocolField(46, 'PARAM_VELOCITY_LIMIT', 'velocity_limit', '速度限制', 'rad/s', 'FLD_F32', 500, None),
    47: ProtocolField(47, 'PARAM_POS_PIDMODE', 'pos_pidmode', '位置环模式', '', 'FLD_U8', 0, [('PVC三环级联(抗干扰)', 0), ('PC双环(高响应)', 1)]),
    48: ProtocolField(48, 'PARAM_PLKP', 'plkp', '位置环Kp', '1/s', 'FLD_F32', 0.0, None),
    49: ProtocolField(49, 'PARAM_PLKI', 'plki', '位置环Ki', '1/s²', 'FLD_F32', 0.0, None),
    50: ProtocolField(50, 'PARAM_PLKD', 'plkd', '位置环Kd', '', 'FLD_F32', 0.0, None),
    51: ProtocolField(51, 'PARAM_PLALPHA', 'plalpha', '位置环滤波系数', '0~1', 'FLD_F32', 0.5, None),
    52: ProtocolField(52, 'PARAM_POS_LIMIT_MODE', 'pos_limit_mode', '位置限制模式', '', 'FLD_U8', 0, [('禁用', 0), ('最小位置限制', 1), ('最大位置限制', 2), ('双端位置限制', 3)]),
    53: ProtocolField(53, 'PARAM_POS_MIN_LIMIT', 'pos_min_limit', '最小位置', 'rad', 'FLD_F32', -15000, None),
    54: ProtocolField(54, 'PARAM_POS_MAX_LIMIT', 'pos_max_limit', '最大位置', 'rad', 'FLD_F32', 15000, None),
    55: ProtocolField(55, 'PARAM_MIT_KP', 'mit_kp', 'MIT 刚度(Kp)', 'N·m/rad', 'FLD_F32', 0.0, None),
    56: ProtocolField(56, 'PARAM_MIT_KD', 'mit_kd', 'MIT 阻尼(Kd)', 'N·m·s/rad', 'FLD_F32', 0.0, None),
    57: ProtocolField(57, 'PARAM_MIT_KPMAX', 'mit_kpmax', 'MIT 最大刚度', 'N·m/rad', 'FLD_F32', 0.0, None),
    58: ProtocolField(58, 'PARAM_MIT_KDMAX', 'mit_kdmax', 'MIT 最大阻尼', 'N·m·s/rad', 'FLD_F32', 0.0, None),
    59: ProtocolField(59, 'PARAM_MIT_TMAX', 'mit_tmax', 'MIT最大扭矩', 'N·m', 'FLD_F32', 0.0, None),
    60: ProtocolField(60, 'PARAM_FWLKP', 'fwlkp', '弱磁Kp', '', 'FLD_F32', 0.0, None),
    61: ProtocolField(61, 'PARAM_FWLKI', 'fwlki', '弱磁Ki', '', 'FLD_F32', 0.0, None),
    62: ProtocolField(62, 'PARAM_TRAJ_MODE', 'traj_mode', '轨迹规划器模式', '', 'FLD_U8', 0, [('禁用', 0), ('梯形轨迹(斜坡缓冲)', 1), ('曲线轨迹(双重缓冲)', 2)]),
    63: ProtocolField(63, 'PARAM_TRAJ_LIMIT_D1', 'traj_limit_d1', '轨迹一阶限幅', 'unit/s', 'FLD_F32', 0.0, None),
    64: ProtocolField(64, 'PARAM_TRAJ_LIMIT_D2', 'traj_limit_d2', '轨迹二阶限幅', 'unit/s²', 'FLD_F32', 0.0, None),
    65: ProtocolField(65, 'PARAM_TRAJ_LIMIT_D3', 'traj_limit_d3', '轨迹三阶限幅', 'unit/s³', 'FLD_F32', 0.0, None),
    66: ProtocolField(66, 'PARAM_TRAJ_TOLERANCE', 'traj_tolerance', '轨迹容差', '0~1', 'FLD_F32', 0.0, None),
    67: ProtocolField(67, 'PARAM_UNDERVOLTAGE_THRESHOLD', 'undervoltage_threshold', '欠压阈值', 'V', 'FLD_F32', 20, None),
    68: ProtocolField(68, 'PARAM_OVERVOLTAGE_THRESHOLD', 'overvoltage_threshold', '过压阈值', 'V', 'FLD_F32', 26, None),
    69: ProtocolField(69, 'PARAM_OVERTEMP_THRESHOLD', 'overtemp_threshold', '过温阈值', '℃', 'FLD_F32', 80, None),
    70: ProtocolField(70, 'PARAM_PRO_TOLERANCE', 'pro_tolerance', '保护超限容忍度', '0~1', 'FLD_F32', 0.1, None),
    71: ProtocolField(71, 'PARAM_PRO_TOLERANCE_TIME_MS', 'pro_tolerance_time_ms', '保护超限容忍时间', 'ms', 'FLD_F32', 1, None),
    72: ProtocolField(72, 'PARAM_CAN_RATE', 'can_rate', 'CAN速率', '', 'FLD_U8', 0, [('1Mbps', 0), ('500Kbps', 1)]),
    73: ProtocolField(73, 'PARAM_CAN_ID', 'can_id', 'CAN ID', '', 'FLD_U32', 1, None),
    74: ProtocolField(74, 'PARAM_CAN_MODE', 'can_mode', 'CAN模式', '', 'FLD_U8', 0, [('静默模式', 0), ('响应模式', 1)]),
}
PARAM_BY_NAME = {field.id_name: field for field in PARAMETERS.values()}

OBSDATA = {
    1: ProtocolField(1, 'DATA_CURRENT_U', 'current_u', 'U相电流', 'A', 'FLD_F32', 0.0, None),
    2: ProtocolField(2, 'DATA_CURRENT_V', 'current_v', 'V相电流', 'A', 'FLD_F32', 0.0, None),
    3: ProtocolField(3, 'DATA_CURRENT_W', 'current_w', 'W相电流', 'A', 'FLD_F32', 0.0, None),
    4: ProtocolField(4, 'DATA_VOLTAGE_Q', 'voltage_q', 'Q轴电压', 'V', 'FLD_F32', 0.0, None),
    5: ProtocolField(5, 'DATA_VOLTAGE_D', 'voltage_d', 'D轴电压', 'V', 'FLD_F32', 0.0, None),
    6: ProtocolField(6, 'DATA_CURRENT_ALPHA', 'current_alpha', 'α轴电流', 'A', 'FLD_F32', 0.0, None),
    7: ProtocolField(7, 'DATA_CURRENT_BETA', 'current_beta', 'β轴电流', 'A', 'FLD_F32', 0.0, None),
    8: ProtocolField(8, 'DATA_CURRENT_Q', 'current_q', 'Q轴电流', 'A', 'FLD_F32', 0.0, None),
    9: ProtocolField(9, 'DATA_CURRENT_D', 'current_d', 'D轴电流', 'A', 'FLD_F32', 0.0, None),
    10: ProtocolField(10, 'DATA_CURRENT_Q_REF', 'current_q_ref', 'Q轴电流参考', 'A', 'FLD_F32', 0.0, None),
    11: ProtocolField(11, 'DATA_CURRENT_D_REF', 'current_d_ref', 'D轴电流参考', 'A', 'FLD_F32', 0.0, None),
    12: ProtocolField(12, 'DATA_VELOCITY', 'velocity', '速度', 'rad/s', 'FLD_F32', 0.0, None),
    13: ProtocolField(13, 'DATA_VELOCITY_REF', 'velocity_ref', '速度参考', 'rad/s', 'FLD_F32', 0.0, None),
    14: ProtocolField(14, 'DATA_THETA_ELEC', 'theta_elec', '电角度', 'rad', 'FLD_F32', 0.0, None),
    15: ProtocolField(15, 'DATA_THETA_MECH', 'theta_mech', '机械角度', 'rad', 'FLD_F32', 0.0, None),
    16: ProtocolField(16, 'DATA_POSITION', 'position', '位置', 'rad', 'FLD_F32', 0.0, None),
    17: ProtocolField(17, 'DATA_POSITION_REF', 'position_ref', '位置参考', 'rad', 'FLD_F32', 0.0, None),
    18: ProtocolField(18, 'DATA_STATE', 'state', '状态', '', 'FLD_U8', 0, [('INIT', 0), ('IDLE', 1), ('TUNING', 2), ('FAULT', 3), ('WARNING', 4), ('开环模式', 5), ('电流模式', 6), ('PID速度模式', 7), ('PID位置模式', 8), ('MIT模式', 9)]),
    19: ProtocolField(19, 'DATA_FAULT', 'fault', '故障', '', 'FLD_U8', 0, [('NONE', 0), ('观测器初始化失败', 1), ('总线通信初始化失败', 2), ('电阻整定失败', 3), ('相电阻不平衡', 4), ('电感整定失败', 5), ('磁链整定失败', 6), ('观测器校准失败', 7), ('极对数不匹配', 8), ('电机堵转', 9), ('机械参数校准失败', 10), ('过电压', 11), ('低电压', 12), ('过电流', 13)]),
    20: ProtocolField(20, 'DATA_WARNING', 'warning', '警告', '', 'FLD_U8', 0, [('NONE', 0), ('过温', 1), ('超速', 2), ('位置超限', 3), ('总线通讯异常', 4), ('观测器运行异常', 5), ('非法输入', 6)]),
    21: ProtocolField(21, 'DATA_VBUS', 'vbus', '母线电压', 'V', 'FLD_F32', 0.0, None),
    22: ProtocolField(22, 'DATA_DRIVER_TEMP', 'driver_temp', '驱动温度', '°C', 'FLD_F32', 0.0, None),
    23: ProtocolField(23, 'DATA_MOTOR_TEMP', 'motor_temp', '电机温度', '°C', 'FLD_F32', 0.0, None),
}
OBS_BY_NAME = {field.id_name: field for field in OBSDATA.values()}

COMMANDS = {name: {'id': value, 'comment': getattr(CmdId, name).__doc__ or ''} for name, value in vars(CmdId).items() if name.startswith('CMD_')}
FEEDBACKS = {name: {'id': value, 'comment': '反馈'} for name, value in vars(FbId).items() if name.startswith('FB_')}
FRAME = {name: {'id': value, 'comment': ''} for name, value in vars(UartFrame).items() if name.startswith('UART_')}
CMD_TARGETS = {}
MAX_PAYLOAD = 128
SCHEMA_VERSION = 1
