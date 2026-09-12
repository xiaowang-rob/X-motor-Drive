# 项目结构

## demo 存放一些其他的控制芯片用can控制xdr的实例代码

## docs 存放一些使用文档或者说明文档或数据手册什么的

## firmware 存主要的app固件和bl固件

### app app固件
### bootload bl固件
### board 板级：HAL/CubeMX/USB 厂商层 + drv 芯片驱动 + startup
### firmware_out 存放输出的hex、bin文件
### tools 存放编译脚本

## protocol 存放上下位机通讯协议

## Software 存放上位机

## README 介绍工程

---

# 分层架构评审

> **评审快照**：`HEAD = dbac051`（含当时工作区未提交的高层重构：`dev_board.*` 已删、新增 `device_cfg.*`、`slot_con.*` 在改）
> **评审方式**：静态阅读 `firmware/{app,bootload,board/xdr_p_app/drv}` 全部源码，未运行硬件实测
> **一句话结论**：分层骨架成立，但 **abs ↔ drv ↔ 高层之间目前是三套并存的 API**，热路径上存在若干效率损失与正确性隐患。

## 一、总体判断

| 层 | 分配（职责划分） | 写法（效率/健壮性） |
|---|---|---|
| 底层抽象 `app/abs` | 思路对，**契约不统一** | 中 |
| 驱动层 `board/<board>/drv` | **清晰** | 偏低（多处会卡死/越界） |
| 高层 `app/{ctl,srv,app}` | **层与层没接上** | 中（实时设计有亮点） |

## 二、底层抽象层 `app/abs`

### 做得好的

- ops 表 + 不透明句柄的多态；`device.h` 统一 `eDeviceStatus`。
- `tEncoder` 把"原始角 → 多圈/零位/M-T/PLL/有效性"收在 abs，业务侧只吃物理量——正确的抽象切分。
- `flash` 的**日志式顺序追加 + 写满才擦除**（见 `flash.h`），磨损友好，比"原地改写"高一个层次。
- 查询接口全用 `static inline`（`encoder.h:100-106`），零调用开销。

### 存在的问题

1. **9 个 ops 契约里 4 个没有实例上下文**（最该统一的一处）：

   | ops | 上下文 |
   |---|---|
   | `tEncoderDriverOps` / `tFlashDriverOps` / `tLedDriverOps` / `tRgbDriverOps` | `void *handle` ✅ |
   | `tSampleMcuOps` | `void *ctx` + 每函数首参 ✅ |
   | `tGateDrvOps` / `tBusDriverOps` / `tUartDriverOps` / `tIAPDriverOps` | **无** ❌ |

   后 4 个**天生只能单例**——想挂 CAN1+CAN2 或两路 UART 就无法复用同一套 ops 实现；两种 ctx 风格（`ctx` vs `handle`）也分裂。

2. **头文件里定义函数体**：`gate_drv.h:27` / `:31` 直接写 `{ return ...; }` 且**不是 inline** → 多编译单元重复定义。

3. **abs 层用堆**：`flash.c:90` `malloc(sizeof(tFlashUnit))`——破坏"纯逻辑 / 可 host 编译 / 确定性"底线，`flash_unit_register` 应改为调用方传静态存储。

4. **命名与注释脱节**：`encoder.h:82` 注释写 `data_valid`，实现里字段叫 `valid_counter`（`encoder.c:31` 残留 `enc->data_valid`）；`encoder.c:136` 出现 `edata_validnc->angle_abs`（改名事故）。

5. **`tIAPDriverOps` 六函数成对重复**：`app_erase/app_write/app_read/bl_erase/bl_write/bl_read` 本可用"分区描述 + 参数"压掉一半。

6. **`fmodf` 进了热路径**：`math_fast.h:44/55` 的 `normalize_angle_2pi/pi` 用 `fmodf`，被 PLL（每中频周期 2 次）与电角度归一调用；M4 上 `fmodf` 是几十~上百周期，改用常量倒数 + 取整（或 `[0,2π)` 定点表示）可省掉大部分。

## 三、驱动层 `board/<board>/drv`

### 做得好的

- v2"直连 HAL、不做中间适配"在板少的前提下省事且高效。
- 中断回调"单文件唯一持有"执行得不错（`sen_mcu.c` ADC、`led_ws28xx.c` PWM_PulseFinished、`bus_can.c` RxFifo0）。
- 实例判别规范：`sen_mcu.c` 判 `hadc->Instance == ADC2`、`led_ws28xx.c` 判 `htim->Instance`。

### 存在的问题（按危害排序）

1. **ISR 里直接回调上层，自己建的队列没用上**：`bus_com.h` 开篇即写"接收数据进内存池 → 地址进队列 → 主线程提取"，但 `bus_can.c:16-25` 在 CAN 中断里直接 `s_rx_cb(...)`；`uart_mcu.c`、`uart_usb_cdc.c` 同样。`queue.c` / `memory_pool.c` 已建好却**根本没接线**。后果：上层耗时的回调直接压在通讯 ISR 里，重活即丢帧。

2. **`fla_mcu.c` 扇区表本身是错的**（数据完整性问题）：
   - F405 只有 12 个扇区（0–11），但 `USR_SECTOR_ID[3] = {10,11,12}` 用了**不存在的扇区 12**；
   - `APP_SECTOR_ID[MCU_NUM_SECTOR_APP]`（宏 = 7）却给了 **8 个初值** → 越界。

3. **`fla_mcu.c` 两处会崩 / 会锁死**：
   - `mf_write` 用 **`uint16_t i`** 索引 `len`（uint32_t）→ IAP 写 >64KB 固件必然回绕；
   - `mf_erase_usr_sector` 失败分支**漏了 `platform_enable_irq()`** → 擦除失败后**中断永久关闭**；
   - `mf_read` **逐字节 `*(volatile uint8_t*)`** → IAP 校验 800KB 会非常慢，应改字读 / `memcpy`。

4. **编码器 SPI 是隐藏瓶颈**：`encoder_drivers.c:39` 预分频**写死 `SPI_BAUDRATEPRESCALER_32`**（APB1 42MHz → **1.3MHz**），16bit 读角约 **30µs**，而 PWM 周期才 50µs（20kHz）——**占周期 60%**；`HAL_SPI_TransmitReceive` 超时 **100ms**，在控制路径上是阻塞风险；且内外编码器 CS **都写成 PA15**（复制未改）。

5. **堆分配实例**（6 处）：`enc_as5047` / `enc_mt6816` / `enc_mt6835` / `led_ws28xx` / `fla_w25qxx` 都用 `calloc` 建 ctx 并配 `create/destroy`。对固定单例硬件，**静态实例更快、更确定**，省掉堆依赖、NULL 检查与整套 destroy API。

6. **判空 / 判实例不一致**：`bus_can.c:22` 判了 `if (s_rx_cb)`；`uart_mcu.c`、`uart_usb_cdc.c` **没判** → 空指针风险。`bus_can.c` 还**忽略 `hcan` 参数**（多路 CAN 会串），而 ADC/PWM 都判了。

7. **宏名 / 函数名不统一造成的编译错误**：`ENCODER_SPI_CH` vs `ENCODER_HSPI`、`platform_disable_irq` vs `plat_disable_irq`、`RGB_PWM_CHANNEL` vs `RGB_PWM_CHANNEL1`、`USB_CS_GPIOx` vs `USB_DP_GPIOx`、`PWM_GET_HTIM` vs `SAMPLE_PWM_HTIM`。

8. **命名留痕**：文件注释仍是旧名（`gate_fd6288q.c` 写 motor_drv、`encoder_drivers.c` 写 enc_spi_engine、`fla_mcu.c` 写 mcu_flash_drv）；`uart_drivers.h` 里**混进了 CAN 的声明**。

## 四、高层 `app/{ctl,srv,app}`

### 做得好的

- **分频时隙调度**（`slot_con` + `high/medium/low_update`）：把高/中/低频任务**错位分摊到不同 PWM 周期**，避免单周期耗时尖峰——本工程最漂亮的一处实时设计。
- FOC 状态机（IDLE/ENABLE/RUNNING/TUNE/FAULT）+ 故障锁存（`foc_main.c:113-121`）。
- PI/PID 连续域 → 离散域转换规范（`pid.c:52` `kd = kd_cont/dt`）。

### 存在的问题

1. **ctl 层与 abs 层是两套 API，完全对不上**（分层最大的断裂）：

   | ctl 调用 | abs 实际提供 |
   |---|---|
   | `encoder_init((eEncoderChip)…)` | `encoder_init(tEncoder*, ops, handle, eEncoderType)` |
   | `encoder_get_angle_inc()` | `encoder_get_position(tEncoder*)` |
   | `encoder_get_vel()` | `encoder_get_velocity(tEncoder*)` |
   | `encoder_pll_update(dt)` | `encoder_pll_update(tEncoder*, dt)` |
   | `encoder_set_angle_zero()` | `encoder_set_zero(tEncoder*)` |
   | `encoder_get_num_turns()` | `encoder_get_turns(tEncoder*)` |

   其中 `eEncoderChip` **类型根本不存在**。即 ctl 仍在调用一套**未实现的旧 bsp 全局接口**。

2. **新组装层 `device_cfg` 退化**：
   - 在**头文件里定义全局变量**（`tFlash flash_mcu; tIAP iap_app; …`）→ 每个包含它的 TU 都来一份 tentative definition；
   - **app 层直接 include 板级 `*_drivers.h`** 并**硬编码板级型号**（`MT6816_create()`、`gate_fd6288q_ops`、`mcu_flash_driver_ops`）→ app 与 board **编译期强耦合**，换板必须改 app。该文件本质是"板专属组装"，更应放 `board/<board>/` 下；
   - `device_cfg.c` 有 `flash_init(&flash_mcu, &mcu_flash_driver_ops, void);`、`led_init(&led0, &g_led_drv_ops)`（缺 `)` 与 `;`），且**文件后半段残留整篇旧 `dev_board.c`**（拼贴事故）；
   - 头文件 guard 仍为 `__DEV_BOARD_H`，且残留 `g_dev` / `tDevBoard` / `dev_board_init` 旧声明。

3. **主循环全速空转**：`app_main.c` 的 `while(1)` **没有 `__WFI()`、没有统一节拍**，各 task 各自判期 → CPU 空耗、发热。电机控制器应加 `__WFI()` 或统一 tick 调度。

4. **状态机跑在 20kHz PWM 中断里**：`foc_main.c` 的 `loop_main_update_task` 在 ISR 中跑 switch，`bsp_pwm_enable/disable` 也在 ISR 里执行。常规做法是**中断只跑控制律、状态机放主循环**，现状是实时性与可维护性双输。

5. **`__DEBUG__` 测量本身污染被测量**：中断里 4 次 `bsp_get_tick_us()`、主循环每轮 2 次；`IF_time.c:26` 的 `time_get_us()` 每次都做 `DWT->CYCCNT / (SystemCoreClock/1000000)`，热路径上应预计算倒数。

6. **重复 / 并存**：
   - `normalize_angle_360`（ctl）与 `normalize_angle_2pi/pi`（utils）并存；
   - `sqrtf` 与 `arm_sqrt_f32` 混用；
   - `bsp_*` 与 `platform_*` 两套平台接口并存（还有 `plat_disable_irq` 第三种写法）；
   - CAN/UART/USB 三个回调签名各不相同（`can_rx_data_callback(u8*, u8)` 连 id 都没有），且与 `bus_com.h` / `uart_com.h` 中定义的均不一致。

7. **`uart_process_frame` 按值返回 128+ 字节结构体**（`uart_com.h:51`）→ 每次调用一次大拷贝。

## 五、跨层共性问题

1. **`u8/u16/u32` 没有任何一处统一定义**——`math_fast.h`、`port_mapping.h`、`svpwm.h`、`log.h`、`fla_mcu.c` 到处在用；`math_fast.h:108` 的 `crc8(const u8*, u8)` 让该错误**扩散到 17 个包含者**。应在一个 `types.h` / `device.h` 中定义一次。
2. **常量重复 tentative 定义**（`fla_mcu.c` 中 `const uint32_t SECTOR_BOUNDS[13];` 先声明后又初始化）。
3. **文件改名留痕**：注释、include guard、宏名仍是旧名字。

## 六、效率优化清单（按 ROI 排序，均不涉及架构改动）

| # | 项 | 位置 | 收益 |
|---|---|---|---|
| 1 | 堆分配 → 静态实例，去掉 `create/destroy` | `enc_*` / `led_ws28xx` / `fla_w25qxx` | 去碎片 / 去不确定性，省 API |
| 2 | ISR 里接上已建好的内存池 + 队列 | `bus_can.c` / `uart_mcu.c` / `uart_usb_cdc.c` | 回调移出 ISR，防丢帧（设计已存在，只差接线） |
| 3 | 编码器 SPI 预分频 32 → 8/16，超时 100ms → 1ms | `encoder_drivers.c` | 读角 30µs → <8µs |
| 4 | `fmodf` 归一 → 倒数乘 / 定点 | `math_fast.h` | 每次省几十~上百周期 |
| 5 | `mf_read` 逐字节 → 字读 / `memcpy`；修 `uint16_t i` | `fla_mcu.c` | IAP 校验快数倍，修 >64KB 崩溃 |
| 6 | 主循环加 `__WFI()` | `app_main.c` | 降功耗 / 发热 |
| 7 | PWM 比较值直写 CCR（绕过 `__HAL_TIM_SetCompare` 三层调用 + 宏分支） | `gate_fd6288q.c` | 每周期省几十周期 |
| 8 | `slot_con_update` 遍历 → 直接索引 | `slot_con.c` | 每中断省 2+10+10 次比较 |
| 9 | `device_cfg.h` 里变量定义 → `extern` + 单一定义 | `device_cfg.h` | 消除重复定义 |
| 10 | `time_get_us` 预计算倒数 | `IF_time.c` | 热路径去除法 |

## 七、结论与建议动作

- **分层思路（abs 业务对象 / drv 直连 HAL / slot 分频调度）是站得住的**，尤其 slot 分频与 flash 日志式单元是有质量的设计。
- **但"高效"目前还谈不上**，三个层面各有硬伤：
  1. **抽象层**：ops 契约不统一（4/9 无上下文）、头文件定义函数、abs 用堆；
  2. **驱动层**：ISR 直接回调（队列白建）、`fla_mcu` 扇区表越界 + 中断漏恢复 + `uint16_t` 长度溢出、编码器 SPI 占周期 60%；
  3. **高层**：**ctl 与 abs 的 API 完全没对齐**，组装层退化且硬编码板级型号，主循环空转。
- **最关键的结构性动作只有一个**：把 ctl 从旧的 `encoder_*` 全局 API 切到 `tEncoder` 对象；其余问题大多是局部可修的。
