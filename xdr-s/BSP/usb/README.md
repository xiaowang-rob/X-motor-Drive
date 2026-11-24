# XDr-S USB Vendor Bulk

## 设备与端点

XDr-S 应用模式使用 TinyUSB Vendor Device，VID/PID 为 `0483:5760`。
配置值为 1，Vendor 接口为 0、alternate setting 为 0，接口类为 `0xFF`。
OUT 端点为 `0x01`，IN 端点为 `0x81`，均使用 Full Speed Bulk，最大包长为 64 字节。
Windows 可使用 WinUSB 驱动，上位机可通过 libusb 或 PyUSB 访问。

## 线上帧格式

| 偏移 | 字节数 | 字段 |
| --- | ---: | --- |
| 0 | 2 | `A5 5A` |
| 2 | 1 | 命令 ID |
| 3 | 2 | 数据长度 N，小端，0 到 4095 |
| 5 | N | 数据 |
| 5 + N | 2 | CRC16，小端 |

CRC16 使用 CCITT-FALSE，poly 为 `0x1021`，初值为 `0xFFFF`，输入与输出均不反射，xorout 为 0。
校验覆盖 5 字节帧头与数据。无数据命令的线长为 7 字节。

## 上位机收发

1. 打开对应的 `0483:5760` 设备，选择配置 1 并 claim 接口 0。向 `0x01` 写入完整编码帧，检查实际写入长度。写入长度刚好是 64 的整数倍时可以直接结束，固件按帧内长度解析。
2. 在独立接收循环持续读取 `0x81`。PyUSB 同步读取建议每次请求 **64 字节**，设置有限超时，空读、超时或收到 ZLP 时继续读取。这样收到一个满包就可以交给解析器。使用 libusb 异步批量读取时，超时回调也要把 `actual_length` 对应的已收字节交给解析器。
3. 每次读取的数据追加到跨调用的缓冲区。搜索 `A5 5A`，如果缓冲末尾只有 `A5`，保留到下次读取。收到 5 字节头后读取小端长度，等缓冲达到 `N + 7` 字节再校验。校验通过就取出一帧并继续解析后续数据，长度或 CRC 错误时从当前帧头向后滑动一个字节重新搜索。
4. 收到短包、ZLP 或一次读取超时时，保留半帧。连续 1000 ms 没有新字节时清理半帧，断开设备时清空缓冲区。命令应答匹配和超时由上层处理，协议本身没有请求序号或自动重传。

TinyUSB 启用了 256 字节 RX FIFO 和 8192 字节 TX FIFO。单次 USB 读取可以包含半帧或多帧，FIFO 中的连续回包也可以共享一个 Bulk 包。TinyUSB 在队列排空且上一笔发送长度正好是端点包长的整数倍时补 ZLP。

## 固件接入

`Get_USB_VendorHandle()` 提供 `tUsbOps`。发送成功表示完整编码帧已进入 TinyUSB 发送路径，FIFO 空间不足时返回失败。接收时 BSP 重组并校验后，以 `[cmd][payload]` 交给 abs 的 USB 接收队列。

固件集成 TinyUSB 时，板级启动须跳过 `MX_USB_PCD_Init()`，由 `usb_start()` 打开 TinyUSB。将 USB HP 和 LP 的中断入口转发给 `bsp_usb_irq_handler()`。应用现有的 `usb_process_frame()` 会通过 abs 的轮询接口推进 TinyUSB 事件。调用方须确保 `usb_start()` 确实执行，并在主循环持续调用接收轮询。

## DFU 模式

`BSP/dfu` 提供标准 TinyUSB DFU 下载模式，VID/PID 为 `0483:5761`，使用接口 0、alt 0、1024 字节传输块。
由启动程序在 USB 初始化之前调用 `bsp_dfu_start()` 并提供 `tDfuDevHandle`，然后在主循环执行 `bsp_dfu_task()`。
目标负责擦除、写入、镜像验证与提交。DFU 驱动检查块顺序、分区边界和下载状态，USB 和 DFU 回调在主循环执行。

当前应用链接到 `0x08000000`，现有 Flash 分区也没有预留独立启动程序空间。启用实机升级前需要先落实引导入口、应用分区和写入目标。设备进入 DFU 模式后，可用标准 `dfu-util -d 0483:5761 -a 0 -D firmware.bin` 下载。
