# 分层架构 v2（drv 直连厂商库 · 已落地）

> v2 替代 v1（usr/if 接口表 + board/hw 适配器）。**已落地**于主树（未提交）。
> 动机：板少、自维护；换板改驱动/每板 platform 更省事；标准库板用"兼容 HAL 封装"吸收。
> 底线保留：abs↔drv 的语义化 ops 契约（业务对象纯逻辑、可 host 编译）。

---

## 1. 两条边界

| 边界 | 处置 |
|---|---|
| 业务边界 abs↔drv（tEncoderDriverOps / tSampleIf / tTimeIf 类型归 abs） | **保留**：业务与芯片协议隔离，业务可 stub 测 |
| 硬件边界 drv↔厂商库 | **取消抽象**：驱动直接 include 厂商库（经每板 platform.h） |

## 2. 目录（已落地形态）

```
usr/
├── abs/   device.h  time.h(tTimeIf)  encoder  led  flash  sense(含 tSampleIf)
│          ├── .c 纯逻辑业务对象            [usr_abs 库，可 host 编译]
├── drv/   enc_spi_engine  as5047  mt6816  mt6835  w25qxx  ws28xx
│          led_drv  sense_drv + *_drivers.h [usr_drv 库，直调 HAL]
└── app/   dev_board.c/h → g_dev            [usr_app 库]
board/xdr_p_o1.2/
├── platform.h/.c   总头+服务：HAL 总头/CubeMX 外设头、外设/引脚宏、
│                   时间(get_ms/us/init/delay)
├── board_config.h + Core/Drivers/…（不变）
```

## 3. 每层要点

| 层 | 要点 |
|---|---|
| usr/abs | 纯逻辑；tTimeIf/tSampleIf 类型本地定义（abs↔drv 契约） |
| usr/drv | 芯片协议 + 直接 HAL；工厂**无参**；外设/引脚经 `platform.h` 宏；时间用 `platform_get_ms/us` |
| platform | 每板唯一"厂商库入口"：include 厂商库/CubeMX 头 + 引脚宏 + 时间服务 |
| dev_board | `platform_init()` → 无参工厂 → abs init；平台裸函数包成 tTimeIf 注入 |

## 4. 关键约定

- **中断回调单文件唯一**：`HAL_ADC_ConvCpltCallback` 在 `usr/drv/sense_drv.c`；
  `HAL_TIM_PWM_PulseFinishedCallback` 在 `usr/drv/ws28xx.c`（同一回调不得多处定义）。
- 编码器 SPI 模式/时序集中在 `usr/drv/enc_spi_engine.c`（直连 SPI3+CS）。
- 驱动"本板资源"都写成 `platform.h` 宏引用 → 换板只换 platform.h/.c；
  换标准库板 = 新板 platform.h include 其 `HAL 兼容封装`，驱动源码不变。

## 5. 验证

- 全目标（usr_abs / usr_drv / board_hw / usr_app + stm32_*）构建 **error/warning = 0**；
- `usr/abs/*.c` host gcc 编译通过（纯逻辑红利保留在业务侧）；
- 代码层已无 `usr/if`、`board/hw` 引用。

## 6. 与 70f9346 的功能缺口对照（已回填至 ★）

对照基线 = 重构前最后一次完整固件提交 `70f9346`。

| 功能 | 70f9346 载体 | 现状 |
|---|---|---|
| 编码器协议+多圈/PLL/零位 | drv/abs | **已迁移**（v2 直连） |
| LED/RGB/呼吸、GPIO LED | bsp_led/rgb + drv/leds | **已迁移** led_drv/ws28xx |
| 电流/Vbus/温度采样+零点 | bsp_adc/mcu_adc | **已迁移** sense_drv/sense（ADC MSP 在 Core/adc.c 内，已确认完备） |
| 外挂 SPI NOR 协议 | drv/w25qxx | **已迁移**（直连） |
| tick/us/delay | bsp_base | **已迁移** platform |
| ★ 内部 MCU Flash 介质 | usr/drv/flash_mcu + bsp_flash | **已回填** `usr/drv/mcu_flash_drv.c`（tFlashDriverOps，F405 几何按地址） |
| ★ Flash 单元管理 + IAP | usr/abs/flash_abs | **已回填** `usr/abs/flash`：日志式单元 + tFlashIAP（erase/write/verify app&bl/jump/reset） |
| ★ 固件跳转/复位/向量/IRQ | bsp_flash.jump + bsp_base | **已回填** platform：jump_to_addr/system_reset/set_vector_offset/irq |
| ★ 通讯底层 CAN/UART/USB | bsp_can/uart/usb | **已回填** `usr/drv/can_drv·uart_drv·usb_drv`（usbd_cdc_if USER CODE 接 rx hook） |
| ★ 电机 PWM/12V/FOC 节拍 | bsp_pwm + bsp_gpio(power) | **已回填** `usr/drv/motor_drv`（12V/compare/使能/上溢采样·下溢 FOC 双钩子） |
| 日志/参数持久化 / FOC 套件 | srv + ctl | **待高层迁移**（现只到底层，文件保留） |

> 底层缺口已全部回填到相应位置（usr/drv / usr/abs / platform）；剩余为高层（ctl/srv/通讯端口）迁移，属后续工作。

## 7. 待办（未完成项）

- 高层（ctl/srv/通讯端口）迁移到 g_dev 与新驱动 API；
- dev_board 之外 usr/app 旧高层文件未编入构建；
- 标准库板 hal_compat 层（未来）；
- 真机联调验证（PWM/采样/CAN/UART/USB/IAP 时序）。
