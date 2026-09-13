# 项目结构

## demo 存放一些其他的控制芯片用can控制xdr的实例代码

## docs 存放一些使用文档或者说明文档或数据手册什么的

## firmware 存主要的app固件和bl固件

### app app固件
```
app/
├── abs/     抽象层：设备契约(ops) + 业务对象（可 host 编译的纯逻辑）
├── app/     组装层 + 通讯端口（高层，重构中）
├── ctl/     控制层：FOC 核心 / SVPWM / 无感(SMO,HFI) / 整定（高层，重构中）
├── srv/     服务层：时隙调度 slot_con / 参数 / 保护 / 状态反馈 / 日志（高层，重构中）
└── utils/   工具：math_fast / crc / pid / filter / queue / memory_pool / trajectory
```
### bootload bl固件
### board 板级：HAL/CubeMX/USB 厂商层 + drv 芯片驱动 + startup

```
board/<board>/
├── drv/      芯片驱动层（直连 HAL）：bus_can / uart_* / sen_mcu / gate_* /
│             led_* / fla_* / enc_* / encoder_drivers / IF_irq / IF_time / IF_config
├── Core/     CubeMX 外设初始化
├── Drivers/  厂商 HAL + CMSIS
├── Middlewares/ USB Device + CMSIS-DSP
├── USB_DEVICE/  USB 应用层（usbd_cdc_if 的 USER CODE 为驱动回调入口）
└── cmake/    工具链文件（gcc-arm-none-eabi.cmake）
```
### firmware_out 存放输出的hex、bin文件
### tools 存放编译脚本

## protocol 存放上下位机通讯协议

## Software 存放上位机

## README 介绍工程

---

# 分层架构约定（当前实现）

> 适用范围：`firmware/app/abs`（抽象层）、`firmware/board/<board>/drv`（驱动层）。
> 高层 `app/{ctl,srv,app}` **仍在重构中，尚未接入本约定**（见"遗留项"）。

## 1. 两层 handle

| | 抽象层 abs | 驱动层 drv |
|---|---|---|
| 结构 | `tBusDriver` `tUartDriver` `tFlash` `tIAP` `tEncoder` `tSense` `tGateDrv` | `tCanBus` `tUartMcu` `tUartUsb` `tSenseAdc` `tGateHw` `tWs28xx` `tLedGpio` `tW25Qxx` `tMcuFlash` `tEncEngine` `tAS5047/tMT6816/tMT6835` |
| 持有 | `ops` 指针、业务状态、队列/内存池**对象** | `ops` 指针、**外设句柄**(hcan/hadc/htim/hspi/huart)、**配置**、运行时缓冲 |
| 位置 | 头文件（跨层契约） | `.c` 内私有 |

三条硬规则：

1. 抽象层只对外暴露**不透明句柄**（`BusHandle` / `UartHandle` / `FlashChipHandle` …，均为 `void *`）；
2. 驱动实例一律**文件内静态**——无 `malloc/calloc`，无 `create/destroy`；
3. 厂商类型（`CAN_HandleTypeDef` 等）只出现在 drv 的 `.c` 里，**共享头文件不被 HAL 污染**。

## 2. ops 契约

- 每个 ops 表的**函数首参 = 该设备的 handle**；**表内不存 ctx / 外设**；
- 需要回调的设备在注册时显式带 ctx：`register_callback(h, cb, ctx)`，由驱动回传，把实例送回抽象层；
- 现有 ops：`tEncoderDriverOps` `tFlashDriverOps` `tLedDriverOps` `tRgbDriverOps` `tSampleMcuOps` `tGateDrvOps` `tBusDriverOps` `tUartDriverOps`。

## 3. 实例获取接口（统一命名）

| 设备 | 接口 |
|---|---|
| CAN | `can_get_handle()` |
| UART（MCU / USB CDC） | `uart_mcu_get_handle()` / `uart_usb_get_handle()` |
| ADC 采样 | `sense_get_handle()` |
| 功率级 | `gate_get_handle()` |
| GPIO LED（2 颗） | `led_get_handle(uint8_t idx)` |
| WS2812 RGB | `rgb_get_handle()` |
| 外部 SPI NOR | `w25_get_handle()` |
| MCU 内部 Flash | `mcu_flash_get_handle()` |
| IAP 配置 | `mcu_iap_config(eIAPtype type)` |
| 编码器 SPI 引擎 | `enc_engine_get_handle()` |
| 编码器芯片（内/外） | `as5047_get_handle(type)` / `mt6816_get_handle(type)` / `mt6835_get_handle(type)` |

> 选择"函数获取"而非 `extern` 实例：`extern` 要求头文件给出**完整类型**，而 handle 结构含厂商类型，会迫使共享头 include HAL、破坏分层。函数返回不透明句柄则不会（且只在组装阶段调用一次，无性能代价）。

## 4. 缓冲归属

大缓冲**由调用方提供**，实例只持指针：

- `tBusBuffer`：内存池缓冲 + 帧队列缓冲（帧队列存**帧实体地址**，按 `sizeof(pointer)` 入队）
- `tUartBuffer`：接收字节队列 + 帧解析缓冲 + 发送组帧缓冲

收益：同一份代码可挂多路总线/串口；实例不再内嵌大数组（uart 实例因此瘦身约 265 字节）。

## 5. 状态归属

- `eDeviceStatus` 由**抽象实例**持有（业务视角）；驱动只返回单次操作的 `bool` 成败；
- **按需设置**：`flash` / `iap` / `encoder` / `bus` / `uart` / `gate` 有状态；`sense` / `led` / `rgb` **无**（纯数据流/纯输出，加状态无意义）；
- 已删除死接口 `tFlashDriverOps.get_state`（历史遗留：定义了但全工程无人调用）。

## 6. 驱动层文件模板

所有 `drv/*.c` 统一为下面骨架（`sen_mcu.c` 为范例）：

```c
// ============================================================
// xxx.c — 名称（板级，直连 HAL）
// （芯片/外设协议说明；实例形态说明；必要 TODO）
// ============================================================
#include "xxx_drivers.h"
#include "...hal 头"

// ---------- 本板配置 ----------
#define ...

// ---------- 实例 handle ----------
typedef struct { const tXxxOps *ops; /* 外设 + 配置 + 运行时 */ } tXxx;

static <ops 函数> 前置声明

const tXxxOps xxx_ops = { ... };

// 静态实例
static tXxx s_xxx = { ... };

XxxHandle xxx_get_handle(void) { return (XxxHandle)&s_xxx; }

// ---- 中断（只在本文件定义） ----
void HAL_xxx_Callback(...) { ... }

// ---- ops 实现 ----
```

## 7. 工具约定

| 用途 | 入口 | 说明 |
|---|---|---|
| 数学 | `utils/math_fast.h` | `FABSF` / `SQRTF` / `CLAMP` / `FSIGN` / `normalize_angle_2pi/pi` / clarke·park；**不裸调** `fabsf/sqrtf/fmodf` |
| 校验 | `utils/crc.h` | `crc8`（帧校验）、`crc32`（固件整区，与 `zlib.crc32` 一致，便于上位机配合） |
| 基础类型 | `abs/device.h` | `eDeviceStatus` + 统一使用 `uintN_t`，**不用** `u8/u16/u32` 别名 |
| 角度归一 | `math_fast.h` | 用"常量倒数 + 取整"替代 `fmodf`（后者是库调用，数十~上百周期） |

---

# 整改记录（原评审问题 → 处置）

> 原评审快照 `HEAD=dbac051`。下表记录本轮整改结果；"✅"= 已修，"⏳"= 明确保留待办。

## 抽象层 `app/abs`

| 原问题 | 处置 |
|---|---|
| 9 个 ops 中 4 个无实例上下文（`gate/bus/uart/iap`），只能单例 | ✅ 全部统一为"函数首参 = handle"，ops 表内不再存 ctx |
| `gate_drv.h` 头文件里定义函数体（非 inline）→ 重复定义 | ✅ 改为 `static inline` |
| abs 层用堆（`flash.c` `malloc`） | ✅ 改为调用方传 `tFlashUnit *` |
| 命名与注释脱节（`data_valid` / `edata_validnc`） | ✅ 修正为 `valid_counter` / `enc` |
| `tIAPDriverOps` 六函数成对重复 | ✅ 合并为 `erase/write/read/jump` + `eIAPtype`；**并进一步复用 `tFlashDriverOps`**（IAP 不再自建读写 ops） |
| `fmodf` 在热路径 | ✅ 改"常量倒数 + 取整" |
| （新）`flash.c` 隐性缺陷 | ✅ 修 5 处：`!s \| !unit` 按位或误用、位图初始化结果被丢弃、`append` 未推进 `free_addr`、`erase` 缺返回值、`unregister` 无效赋 NULL |

## 驱动层 `board/<board>/drv`

| 原问题 | 处置 |
|---|---|
| ISR 里直接回调上层，队列/内存池白建 | ✅ 驱动只回调（带回 ctx），由 **abs 层**入内存池 + 队列；业务逻辑不再跑在通讯 ISR |
| `fla_mcu` 扇区表越界（`{10,11,12}` 与 7/8 个初值不匹配） | ✅ 已修正为 `BL={0,1}` / `APP={2..8}` / `USR={9,10,11}` |
| `fla_mcu` 会崩/会锁死（`uint16_t i` 索引、擦除失败漏恢复中断、逐字节读） | ✅ 全修；`mf_read` 改 `memcpy` |
| 编码器 SPI 是瓶颈（预分频 32 → 1.3MHz，占周期 60%；超时 100ms；内外 CS 同引脚） | ⏳ **保留**（你后续完善）；CS 引脚已加 TODO |
| 堆分配实例（6 处 `calloc` + `create/destroy`） | ✅ 全部静态化；编码器改 `xxx_get_handle(type)`，无 `used` 池 |
| 判空 / 判实例不一致 | ✅ 统一：中断判实例（`htim != s_xxx.htim` 等）、回调判空 |
| 宏名/函数名不一致导致编译错误 | ✅ 统一（`ENCODER_HSPI`、`RGB_PWM_CHANNEL`、`SAMPLE_PWM_HTIM`、`__disable_irq` 等）；并修好 `usbd_cdc_if.c` 把 typedef 名当函数调用的错误（原阻塞 `stm32_usb`） |
| 命名留痕（注释仍是旧文件名） | ✅ 全部文件头注释重写 |
| （新）W25 轮询状态寄存器过密 | ✅ `w25_wait_idle` 改"读状态 → `HAL_Delay(1)`"，每轮只取一次 tick |

## 高层 `app/{ctl,srv,app}`（本轮按要求**未动**）

| 原问题 | 状态 |
|---|---|
| ctl 与 abs 是两套 API（`encoder_get_angle_inc()` vs `encoder_get_position()` 等） | ⏳ **最大待办** |
| 组装层 `device_cfg`：头文件定义变量、硬编码板级型号、与旧 `dev_board.c` 拼贴 | ⏳ |
| 主循环全速空转（无 `__WFI__`） | ⏳ |
| 状态机跑在 20kHz PWM 中断里 | ⏳ |
| `bsp_*` / `platform_*` / `plat_*` 三套平台接口并存 | ⏳ |
| `slot_con_update` 遍历式调度 | ⏳ |

## 跨层

| 原问题 | 处置 |
|---|---|
| `u8/u16/u32` 无统一定义 | ✅ abs / drv / utils 已统一为 `uintN_t`（高层待重构时收口） |
| `device.h` 与 `types.h` 双头 | ✅ 合并为 `abs/device.h` |
| （新）`queue.c` 缺 `<stddef.h>`/`<string.h>` | ✅ 补齐 |
| （新）`memory_pool.c` 无条件 `__enable_irq()`（ISR 中会误开中断） | ✅ 改 PRIMASK 保存/恢复（`__ARM_ARCH` 条件编译，保持 host 可编） |

## 效率清单（原 10 项）

| # | 项 | 状态 |
|---|---|---|
| 1 | 堆分配 → 静态实例 | ✅ |
| 2 | ISR 接内存池 + 队列 | ✅（结构已通） |
| 3 | 编码器 SPI 预分频/超时 | ⏳ |
| 4 | `fmodf` → 倒数乘 | ✅ |
| 5 | `mf_read` 字读 | ✅ |
| 6 | 主循环 `__WFI()` | ⏳（高层） |
| 7 | PWM 直写 CCR | ✅ |
| 8 | `slot_con` 直接索引 | ⏳（高层） |
| 9 | `device_cfg.h` 变量 → `extern` | ⏳（高层） |
| 10 | `time_get_us` 预计算倒数 | ✅ |

---

# 验证与遗留项

## 编译验证（本轮）

```
board_drv（含 stm32_hal / stm32_core / stm32_usb / board_hw）
  → 退出码 0，错误 0，警告 0
app/abs + app/utils（target 语法检查）  → 15/15 通过
utils/crc.c · queue.c · memory_pool.c（host gcc）  → 通过（纯逻辑层无 CMSIS 依赖）
```

## 遗留项

1. **高层接入（唯一的结构性阻塞）**：`ctl` 仍调用一套未实现的旧 `encoder_*` 全局 API；组装层需按上面的约定改为
   - 用 `xxx_get_handle()` 取实例 → `abs` 对象 init；
   - bus/uart 需自备 `tBusBuffer` / `tUartBuffer`；
   - IAP 用 `mcu_iap_config(type)` + `iap_verify_crc(expect_crc)`。
2. **编码器 SPI**（`encoder_drivers.c`）：预分频仍 32（≈1.3MHz）、超时仍 100ms、内外 CS 仍同引脚 —— 均待硬件确认后完善。
3. **采样 DMA 撕裂保护**（`sen_mcu.c` TODO）：循环 DMA 与 FOC 下溢读取之间缺同步，建议改 DMA 半满/全满双缓冲。
4. **中断分发扩展性**（`gate_fd6288q.c` TODO）：现依赖"单文件唯一持有某 HAL 回调"的约定；将来多路 TIM 冲突时需集中分发。
5. **IAP 校验需上位机配合**：`iap_verify_crc` 要求上位机给出分区 CRC32（`zlib.crc32` 同款），协议需带该字段。
