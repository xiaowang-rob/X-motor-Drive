# Motor Control

仅使用现有 `parameter_list.h` 的 `tParameter` 与参数枚举，不修改协议定义。
每个 `tMcMotor` 拥有独立配置快照、指令、传感器、控制器、调制和调度状态，
采用静态分配，无 BSP、services、通信解析或动态内存依赖。

## 结构和当前能力

| 目录 | 职责 |
| --- | --- |
| `core` | 基础数据、配置校验、生命周期组织和多速率调度 |
| `command` | 完整指令校验、限幅、目标映射、轨迹工具的协议参数适配 |
| `controller` | 电流环、速度环、位置 PC/PVC、MIT 和弱磁的组织与电机限幅 |
| `common` | 独立 PI、PID、MIT 阻抗算法、梯形/S 型轨迹工具，无协议和硬件依赖 |
| `foc` | 有效相电流重构、Clarke/Park、反 Park、线性 SVPWM |
| `sensor` | 编码器机械角、连续位置、PLL 速度及一致的方向处理 |
| `output` | PWM 预装载、使能和立即关断 |

数学调用全部通过 `firmware/math/xmath.h`，统一为 `xmath_<运算>_f32/f64`，
数学常量使用 `XMATH_*_F32`。移除 motor 内重复的数学包装；单精度快速环与
双精度轨迹显式区分。固件默认使用 CMSIS-DSP，独立主机测试使用同一 xmath 接口的
LIBM 后端。测试断言中的标准库函数作为独立数值参考保留。

PI 与 PID 是不同结构和独立执行函数。电流 d/q、速度、弱磁环使用 `tMcPi`，
只有位置环使用 `tMcPid`；PI 不保存微分状态，不执行微分计算和首次反馈分支。
`Ki*dt`、`Kd/dt` 与输出范围在控制器初始化时确定，step 只传入目标和反馈。
MIT 的补偿衰减系数也在初始化时计算，运行步进不计算指数函数。
公共 API 和算法使用调用者已保证有效的指针、有限数值、正确初始化和周期，
不在运行时重复检查空指针、NaN/Inf 或初始化标志；限幅与抗饱和是算法本身的一部分。
轨迹工具使用独立配置类型；现有 `tParameter` 仅由 command/controller 适配。

快速路径约定：

- 每周期仅执行一次 Park，sin/cos 缓存给反 Park 使用。
- SVPWM 通过 alpha/beta 分界线比较求扇区，不调用 atan2。
- 闭环不重复执行电压圆限幅，不进行先乘 Vbus 再除 Vbus 的转换。
- 电压限幅先比较模平方，仅饱和时开方；未启用弱磁时不计算弱磁反馈模长和 Id 电流圆开方。
- 电压利用率保存平方值，反馈 API 读取时才开方。
- PLL 系数、方向/零偏、Kt 倒数与控制周期在初始化/配置时预计算。
- stop/start 只复位控制器状态，保留系数；快速中断不重新计算 MIT 的指数衰减。
- 没有新指令时不拷贝邮箱；没有到期任务或指令变更时跳过外环组织函数。

`mc_motor.h` 是新公共接口。旧 `motor_*`、`g_motor`、`slot_con` 和公开内部字段
不保留兼容包装，外部应用/观测服务需要后续适配；本次不修改这些外部文件。
`tMcRunState` 与 `tMcMode` 分离，不能把它们直接强转为协议 `eMotorState`。

TODO(feature): PVT、SMO、HFI、自动角度切换/融合后续实现。原观测器算法保留在
`sensor/observer/reference`，明确排除于编译和闭环执行。请求当前不支持的闭环角度源
返回 `MC_UNSUPPORTED`；不会用无效编码器角度冒充无感反馈。
`motor_type=BLDC` 仍表示正弦 FOC，不实现六步换相。

## 参数和单位

初始化/停机配置时拷贝 `tParameter`，电机不会持续引用可被通信线程写入的全局参数。
`mc_motor_configure()` 只在 IDLE 应用，并重新等待有效采样；配置变更会重置位置零点。
普通 stop/start 则保留位置和编码器跟踪状态。参数保存由外部服务处理。

机械位置/速度采用 rad、rad/s；机械累计位置内部用 double；公共指令位置是 float。
电角速度 = 机械角速度 × 极对数。`theta_offset` 沿用原始机械角零偏语义。
`positive_dir=true` 保持原始方向，false 同时反转机械位置/速度和电角度/速度；
外部编码器适配层不要再次反转。设置位置零点不会改变电角度零偏。

开环指令给定电角度、电角速度、d 轴电压 V，输出按本周期 Vbus 限幅。
闭环电流 PI 沿用旧参数的**归一化电压**增益，PI 输出 1 对应 `Vbus/sqrt(3)`，
dq 矢量一起限幅并向积分器反馈实际输出；闭环直接传归一化电压到调制，
电压反馈和估计使用 V。
因此本次没有把旧增益值直接重新解释为 V/A。弱磁只在速度模式开放，
在快速环中更新，并以额定电流为 dq 电流矢量上限。

TODO(parameter): 下表集中记录当前协议字段的缺失、歧义和本模块不消费的字段。
“不消费”不代表应从设备协议删除，部分字段属于外部驱动/保护/整定服务。

| 状态 | 字段/内容 | 当前处理 |
| --- | --- | --- |
| 缺少 | 独立力矩常数 Kt | 暂沿用 `motor_ke` 为 Nm/A；零值禁止 TORQUE/MIT。需明确峰值/RMS、相/线、机械/电气定义后再迁移 |
| 含义待统一 | `dclkp/dclki/qclkp/qclki` 单位 | 保留归一化增益；现协议 A/V、A/(V·s) 标注不能直接作为电压输出增益使用 |
| 含义待统一 | `motor_rated_current` | 暂作为峰值 dq 电流限幅；缺独立额定值、峰值限流及过流阈值的约定 |
| 缺少 | 控制周期与分频 | 初始化 `tMcTiming` 提供；默认 command/velocity/position/slow 为 fast 的 10/10/50/100 分频 |
| 缺少 | 采样有效性与通道映射 | 由端口返回有效相掩码；通道顺序为 U/V/W，符号由硬件适配完成 |
| 缺少 | ADC/PWM 延迟、角度补偿 | 使用当前采样角度，不猜测延迟，不启用预测补偿 |
| 缺少 | 编码器 PLL 带宽 | 暂固定自然频率 300 rad/s，低采样率时自动降低以维持离散稳定性 |
| 缺少 | MIT 补偿衰减时间常数 | 暂固定 50 ms，按实际 MIT 更新周期指数衰减 |
| 缺少 | 弱磁目标/最大负 Id | 暂用 95% 电压利用率，负 Id 下限为 -rated_current |
| 缺少 | 指令超时、停机斜率 | 指令保持到更新/stop；stop 立即失能，不执行有源制动或减速停机 |
| 歧义 | `traj_limit_d1/d2/d3` | 位置模式分别为 rad/s、rad/s²、rad/s³；速度模式为 rad/s²、rad/s³、rad/s⁴，暂共用一组字段 |
| 非本轮算法输入 | `motor_kv/rs/ld/lq/psif/j/b`、`clbw_coef/dclbw/qclbw` | 保留协议快照；本轮不自动整定、解耦或生成惯量/摩擦前馈 |
| 外部驱动使用 | `hall_*`、`abi_*`、`sin_*`、`cos_*`、`sincos_fl`、`enc_chip` | 电机端口只接收统一机械角，具体芯片配置留给驱动 |
| 待后续实现 | `sensorless/hfi_smo_threshold/abs_pos_enc` | 不实现 SMO/HFI 融合及第二位置编码器；闭环选择无感时返回不支持 |
| 外部服务使用 | `undervoltage_threshold/overvoltage_threshold/overtemp_threshold/pro_tolerance/pro_tolerance_time_ms` | 保护服务监测阈值/容忍时间，并调用 `mc_motor_fault()`；内核仅直接拒绝无效采样/非正母线电压 |
| 非电机控制参数 | `can_rate/can_id/can_mode` | 不读取、不修改 |

`mit_kp/kd` 用于 `mc_command_default()`，指令中的 kp/kd 可更新并受 kpmax/kdmax
限制；MIT 五元组直接执行，**不经过全局轨迹规划**，不会丢弃 vel/tau_ff。
位置模式只限制目标位置，不把软件目标限幅当作机械越限保护；实际位置越限由外部保护报告。

## 电流采样和调制时序

`read_sample(user, applied, sample)` 返回与 `applied` 对应的采样：

- `current[U/V/W]` 为已完成零点偏置校准、有符号的相电流 A。
- `current_valid_mask` 必须为 UV、UW、VW 或 UVW；无效相可为 NaN。
  两相有效时使用 `Iu+Iv+Iw=0` 重构第三相，三相有效时移除共模偏置。
- `pwm_sequence` 必须对应传入的已应用 PWM 序号。实际硬件需要在 ADC 采样时
  锁存此序号及有效相，而不是在读取时简单覆盖序号掩盖时序错误。
- 端口结合实际采样拓扑、`applied.sector` 与占空比确定哪些相有效。
  本模块不猜测原硬件的扇区/通道对应表，也不重复取反带符号电流。
- `bus_voltage` 是有效母线电压 V，`rotor_angle` 是单圈机械角 rad。
  采样间机械旋转需小于半圈，掉帧后无法保证累计位置仍绝对正确。

SVPWM 扇区来自实际 alpha/beta 电压矢量。`write_pwm()` 须同时负责 PWM 预装载
和按新扇区更新下一次采样计划，不能继续使用转子电角度作为采样扇区。
回调返回时视为该序号对应的 PWM 已安排在下一采样周期生效；若板卡流水有额外延迟，
适配层应匹配 ADC 与 PWM 元数据，或者后续扩展明确的队列接口。

快速环先处理电流/编码器，再更新到期的指令/外环，然后电流环、调制和输出。
首个运行周期全部控制器已准备好，不会初始化后清除刚收到的目标。
各分频任务在首周期执行，之后严格每 divisor 个快速周期执行，dt 与对应周期一致。
温度回调在 slow 分频执行，失败时 `temperature_valid=false`。

S 型轨迹采用 C2 连续五次 Bernstein 曲线，以导数控制点凸包验证速度/加速度/jerk
上界。在线改目标时从当前值和导数重新规划；保守校验未找到可行新段时继续旧段，
并保留最新目标重试，可能短暂继续旧方向，不声称时间最优或始终单调。
规划还约束候选时长和位置控制点，避免仅靠延长时间满足导数限制造成过大绕行。
规划每次最多尝试 40 个时长，只在前台 `mc_motor_service()` 执行。前台规划可以
被快速环抢占；提交时检查指令代数及轨迹初值，过期的结果丢弃并返回 MC_BUSY。
移动中若前台始终无法在两次轨迹更新之间完成，允许继续旧段，到静止后再规划最新目标。
新 S 型目标在 service 生成有效段前保持已有目标；快速环仅在 command 分频求值曲线。
梯形轨迹使用加速度受限的在线规划，容差内终点吸附。MIT 不使用轨迹。

## 生命周期与并发约定

1. 静态分配 `tMcMotor`，在 tick 未开始前 `mc_motor_init()`；初始化即关闭驱动并写零矢量。
2. 定时调用 `mc_motor_tick()`；IDLE 时也采样和跟踪位置。
   前台循环调用 `mc_motor_service()`，用于 S 型重规划；CURRENT/MIT 等模式不需要规划。
3. 用 `mc_command_default(mode, &param)` 构造完整指令，赋值后 `mc_motor_submit()`。
4. 首次有效采样后 `mc_motor_start()` 返回 MC_OK；实际使能在下个 tick，先写 PWM 再使能。
5. RUNNING 只接受相同模式的新指令；切换模式先 stop，等待 tick 清理至 IDLE，再提交并 start。
6. stop/fault 在返回前立即关断；fault 锁存，普通 stop 不会清故障。
   修复来源后显式 clear_fault，重新采样、提交指令、start；清故障不会自动重启。

`mc_motor_submit()` 是“最新完整指令”单槽邮箱，多条提交在消费前会合并为最新一条，
不是 PVT 队列。调用方必须在 API 执行期间保持传入参数/指令对象稳定。

中断运行 tick 时，端口必须成对实现 enter/exit_critical，保存并恢复原中断状态；
临界区须排除 tick、指令发布和可调用 stop/fault 的软件中断之间的竞争，
需覆盖所有相关中断优先级（例如保存/恢复 PRIMASK；不能只屏蔽普通任务）。
未提供临界区时所有 API 必须严格串行。tick 不得重入。
configure/clear_fault/zero_position/start/feedback 等管理 API 应由低于 tick 优先级
的上下文调用；故障 ISR 可直接调用 fault，最终 PWM 提交还会再次检查故障和停止状态。
硬件 Break/比较器等快速保护仍由板级实现，软件不替代硬件关断。
端口回调必须有界、不阻塞，写 PWM/使能和临界区回调不得重入电机 API。
算法虽不在整个 tick 上锁，但配置/指令拷贝和状态快照会短暂屏蔽中断，需实测延迟。

```c
static tMcMotor motor;
/* port 由板级实现，参数仍是现有 tParameter。 */
tMcTiming timing = mc_timing_default(1.0f / 20000.0f);
tMcResult rc = mc_motor_init(&motor, &param, &timing, &port);
/* 检查 rc，然后在每个 ADC/PWM 控制周期调用 mc_motor_tick(&motor)。 */
tMcCommand command = mc_command_default(MC_MODE_CURRENT, &param);
command.value.current = 1.0f;
rc = mc_motor_submit(&motor, &command);
/* 检查 rc；有效采样前 start 返回 MC_NOT_READY，稍后再试。 */
rc = mc_motor_start(&motor);
```

## 模块验证

```sh
cmake -S firmware/motor -B /tmp/motor-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/motor-tests
ctest --test-dir /tmp/motor-tests --output-on-failure
```

主机测试使用真实算法、xmath LIBM 后端和假硬件端口，覆盖重构/坐标变换、SVPWM、电压限幅、PID
阻尼/抗饱和、MIT 补偿、轨迹约束/反向改目标、多速率、首周期、故障/异步停机和实例隔离。
这些测试不替代上板电流极性、编码器方向、实际采样窗口、闭环整定及实时预算验证。
