先以 `firmware_new/` 为固件根目录，直接保留 `board/`、`app/`、`bootload/`、`services/`、`xfoc/` 等同级目录。

下面就按照你这次给出的真实目录，重新制定改造步骤。目标是从原项目逐步迁移，不推翻已有 CubeMX 工程。

# 一、确定最终的 Firmware 目录

firmware_new/

app/

公共 APP 业务入口与运行调度，不存放板级 CubeMX 生成文件。

board/

XDr-P、XDr-S 的 APP / BL 工程、CubeMX 文件和 BSP。

bootload/

公共 Bootloader 业务、镜像校验、升级流程和跳转逻辑。

services/

parameters、obsdata、protection（之前的日志服务融合进去）、communication、status_feedback、iap。

xfoc/

原有 abs（仅必要的enc、pwm、sample）、ctl、utils 和控制算法，逐步解耦硬件依赖。

protocol/

固件侧协议代码及生成文件；协议定义源仍由仓库根目录统一维护。

build_tools/

构建、烧录、链接脚本生成及固件检查脚本。

releases/

发布固件和版本相关文件。

# 二、从原目录到新目录的映射

先建立下面这张迁移表，后续就按照表格逐项修改。

| 原项目位置                    | 新项目位置                                   | 改造方式                    |
| ------------------------ | --------------------------------------- | ----------------------- |
| `firmware/board/`        | `firmware/board/`                       | 保留目录名，整理内部工程            |
| `firmware/xfoc/`         | `firmware/xfoc/`                        | 保留目录名，先整理依赖             |
| `firmware/xfoc/srv/`     | `firmware/services/`                    | 根据职责拆分，不能整个目录直接搬迁       |
| 原有公共 APP 业务代码            | `firmware/app/`                         | 集中管理 APP 初始化和后台调度       |
| `firmware/bootload/`     | `firmware/bootload/`                    | 保留目录，明确公共 BL 业务与板级入口的边界 |
| `firmware/protocol/`     | `firmware/protocol/`                    | 保留，明确生成代码与手写适配代码        |
| `firmware/tools/`        | `firmware/build_tools/`                 | 统一构建与发布脚本               |
| `firmware/firmware_out/` | `firmware/releases/`                    | 迁移发布产物，旧构建缓存不迁移         |
| `firmware/build/`        | 构建目录                                    | 清理旧缓存，重新生成              |


# 三、具体改造步骤

## 第 1 步：先冻结原项目的构建状态

暂时不移动代码。

- 给当前项目建立 Git 提交。
- 分别记录 XDr-P APP、XDr-P BL、XDr-S APP、XDr-S BL 的构建情况。
- 保存现有 `.ioc`、链接脚本、启动文件、CMake 配置和固件打包脚本。
- 记录 APP 起始地址、BL 起始地址和升级镜像存储位置。

验收条件： 你能明确知道哪些工程原本可以编译、哪些存在问题，以及现有升级流程如何工作。

## 第 2 步：先把 `board/` 下的四个工程边界整理清楚

你目前的结构是：

```
board/
├── xdr_p/
│   ├── app/
│   │   └── XDr-P.ioc
│   └── bl/
│       └── XDr-P.ioc
└── xdr_s/
    ├── app/
    │   ├── bsp/
    │   ├── cmake/
    │   ├── Core/
    │   ├── Drivers/
    │   ├── Middlewares/
    │   ├── CMakeLists.txt
    │   └── XDr-S.ioc
    └── bl/
        └── XDr-S.ioc
```

目前 XDr-S APP 的 CubeMX 生成文件比较完整，而 XDr-P APP 和两个 BL 目录只显示 `.ioc`。如果这只是目录树没有展开，没关系；如果目录中真的只有 `.ioc`，需要先确认其余生成文件是否存放在其他位置，以及构建脚本是否会自动生成它们。

建议整理后的目录形式为：

```
board/
├── xdr_p/
│   ├── app/
│   │   ├── Core/
│   │   ├── Drivers/
│   │   ├── bsp/
│   │   ├── cmake/
│   │   ├── CMakeLists.txt
│   │   └── XDr-P.ioc
│   └── bl/
│       ├── Core/
│       ├── Drivers/
│       ├── CMakeLists.txt
│       └── XDr-P.ioc
└── xdr_s/
    ├── app/
    │   ├── Core/
    │   ├── Drivers/
    │   ├── bsp/
    │   ├── cmake/
    │   ├── CMakeLists.txt
    │   └── XDr-S.ioc
    └── bl/
        ├── Core/
        ├── Drivers/
        ├── CMakeLists.txt
        └── XDr-S.ioc
```

`bsp/` 只放板级驱动及适配。CubeMX 生成的 `Core/`、`Drivers/`、启动文件和链接脚本仍留在各自工程中。

## 第 3 步：建立公共 APP 入口

新建或整理：

```
app/
├── app_main.c
├── app_main.h
├── app_init.c
└── app_update.c
```

这里不放第二个 `main()`。各板子的 `Core/Src/main.c` 仍然是实际的程序入口。

板级 `main.c` 保留硬件初始化：

```
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    /* CubeMX 生成的外设初始化 */

    if (App_Init() != 0)
    {
        Error_Handler();
    }

    while (1)
    {
        App_Update();
    }
}
```

公共 APP 层负责调用服务和 XFoc：

```
int App_Init(void)
{
    /* 参数与持久化服务初始化 */
    /* 通讯服务初始化 */
    /* XFoc 初始化 */
    /* 状态与保护服务初始化 */

    return 0;
}

void App_Update(void)
{
    /* 通讯消息处理 */
    /* 参数请求处理 */
    /* 观测数据更新 */
    /* 状态反馈 */
}
```

这段代码表达的是分层方式，实际函数和初始化顺序应根据现有驱动依赖确定。

实时控制环不要迁到后台 `App_Update()` 中。 你目前使用的定时器、ADC/DMA、编码器采样和 FOC 控制时序需要保持原样，等架构稳定后再单独优化。

## 第 4 步：拆分 `xfoc/abs`，先处理依赖再移动文件



## 第 5 步：整理 `services/`



## 第 6 步：整理 `bootload/` 和 IAP

你现有的 IAP 相关代码涉及 `bootload/`、`xfoc/abs/iap.c`、板级 BL 工程以及上位机升级脚本。

建议逐步拆分成：

```
bootload/
├── bl_main.c
├── bl_update.c
├── bl_image.c
├── bl_jump.c
└── bl_boot_policy.c
```

公共 BL 业务可以放在这里，但板级 BL 的 `main.c`、启动文件、HAL、链接脚本和硬件驱动仍留在 `board/xdr_p/bl/`、`board/xdr_s/bl/` 中。

原 `iap.c` 按职责拆分：

- APP 侧设置升级标志、响应升级命令：放入 APP 或相应服务。
- Bootloader 接收固件、验证镜像、写入 Flash：放入 `bootload/`。
- 芯片 Flash 擦写实现：由板级驱动提供。
- 上位机升级流程：继续放在 `Software/`。

迁移后必须重新核对 APP/BL 的链接脚本、向量表偏移、镜像打包地址和升级标志，不能只验证编译通过就结束。

## 第 7 步：整理 `protocol/` 和 `build_tools/`

你已经有：

```
protocol/
build_tools/
```

这两个目录可以继续保留，只需要明确职责。

```
protocol/
├── protocol.json
├── gen_protocol.py
└── tests/

build_tools/
├── build.py
├── build_core.py
├── generate_linker_script.py
├── check_flash.py
├── flash_core.py
└── utils.py
```

这里的 `protocol.json`、`gen_protocol.py` 是示例命名，以你现有文件为准，不需要为了统一名字重复创建脚本。

协议源定义建议只保留一份。固件侧 C 头文件和上位机 Python 定义由同一份协议定义生成，或者通过自动化测试检查一致性。

`build_tools/` 则统一选择目标板、APP/BL 类型、工具链、构建目录和输出目录。

## 第 8 步：最后整理 `releases/`

完成前面步骤后，再统一发布产物：

```
releases/
├── xdr_p/
│   ├── app.bin
│   ├── bootloader.bin
│   └── manifest.json
└── xdr_s/
    ├── app.bin
    ├── bootloader.bin
    └── manifest.json
```

这只是建议的发布形式，实际文件名应匹配你已有的上位机升级协议和打包规则。

不要把临时 `.o`、CMake 缓存和中间文件放进 `releases/`。改造完成后，用干净的构建目录重新编译四个固件目标，再做烧录和升级回归测试。

# 四、推荐的实际执行顺序

## 改造进度清单

0/8 已完成

1\. 建立原工程基线

记录四个固件目标的构建结果和 Flash 分区。

2\. 整理 board 工程

确认四个 APP/BL 工程的生成文件与构建入口。

3\. 建立公共 APP 接口

让板级 main.c 调用 App_Init() 与 App_Update()。

4\. 拆分 XFoc 硬件依赖

区分抽象接口、算法和 BSP 实现。

5\. 迁移公共 Services

拆分参数、观测数据、保护、通讯和状态服务。

6\. 整理 Bootloader/IAP

确认镜像校验、Flash 分区和启动跳转。

7\. 统一协议与构建工具

统一生成流程和固件目标配置。

8\. 清理发布目录并回归测试

干净构建、烧录、运行和升级验证。

复制改造清单

我建议你第一轮只做前 3 步。 先让 XDr-P 和 XDr-S 的 APP 入口统一调用公共 `App_Init()`、`App_Update()`，不改控制算法，不改编码器实现，不改 Flash 分区。验证两块板子正常后，再继续拆分 XFoc 和 Services，风险最低。

如果你希望持续推进，我也可以在你完成某一阶段后提醒你继续检查对应的代码迁移、构建和升级兼容性。