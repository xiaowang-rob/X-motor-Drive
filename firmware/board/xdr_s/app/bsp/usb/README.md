# XDr-S USB vendor Bulk

本目录已将运行时 USB 类从 CustomHID 替换为 vendor Bulk，保留历史文件名
`app/usbd_custom_hid_if.c/.h`，不修改 `.ioc` 或 CMake。代码尚未编译、测试或上板验证。

## 设备与端点

- VID/PID 和字符串仍取自 `bsp_cfg.h`；旧字符串中可能仍有 HID 字样。
- 设备/接口 class 为 `0xFF`，接口 0、alternate setting 0。
- OUT `0x01`，IN `0x81`，Full Speed Bulk，最大包长 64B。
- PMA：EP0 OUT/IN 使用 `0x18/0x58`，EP1 IN/OUT 使用 `0x98/0xD8`。
- USB IRQ 优先级为 3；SOF 每毫秒用于发送提交重试和半包超时处理。
- Windows 暂用 Zadig 为此设备绑定 WinUSB；未实现 WCID/MS OS 自动绑定。
  上位机可通过 libusb/pyusb 认领接口 0。此设备不再是 HID 或虚拟串口。

## 线格式 v1

```text
offset  bytes  字段
0       2      A5 5A
2       1      version = 1
3       1      flags = 0（保留）
4       1      seq（每方向独立，modulo 256）
5       2      payload length（小端，1..4096）
7       N      payload
7+N     2      CRC16（小端）
```

CRC16/CCITT-FALSE：poly `0x1021`、init `0xFFFF`、refin/refout=false、xorout=0，
覆盖整个 7B 包头和 payload，不包含 CRC 字段。协议总开销为 **9B**。
USB reset/重新配置清空收发队列和序号，保留已注册的回调与累计统计。
首次有效 RX 序号用于同步；之后序号不连续只计数，不丢包、不自动重传。

主机和设备都必须依据长度重组，不能用 USB read 返回值或短包替代长度字段。
设备发送以短包结束每条消息；线长度恰为 64 的倍数时追加 ZLP，避免较大的
主机 Bulk read 一直等到超时。一次读可以得到半条或多条消息。主机 RX 缓冲建议
至少 8192B，同时持续读写；不要每 64B 做一次同步命令应答。

RX 无新字节 1000ms 后丢弃半包；非法 header/CRC 采用逐字节重新同步。
未配置、队列满或发送重入时 `send` 返回 false。返回 true 只表示已复制入队，
不表示主机已收到；reset/拔线可能丢弃已接受的数据。

## 当前上层兼容边界

`uart_usb_ops` 暂时仍接入既有 `tUartDriver`：一条 USB payload 是完整的
`[head][id][len][payload][crc8][tail]` UART 帧，USB 驱动不修改内部字节。

- `UART_MAX_PKT_SIZE=128` 是业务 payload 上限，完整 UART 帧最大 133B。
- 现有上位机 IAP 分块仍为 120B；本轮不提高块大小或修改 IAP 业务。
- 旧 USB RX 字节队列只有 256B，大消息即使通过 USB 校验仍可能被上层丢弃。
- TX/RX 传输环形缓冲各 8192B，RX payload 暂存 4096B，均为静态内存。
- 当前 RX 校验/复制在 USB 中断上下文完成，回调必须立即复制/入队，不能阻塞。
  后续 `usb_com` 用完整消息队列把业务处理移至主循环；高吞吐和电流环抖动尚未测量。

这不是与现有串口上位机兼容的线协议；上位机需要实现上述 USB 外层封包。

## abs/usb_com 接入

新增 `firmware/xfoc/abs/usb_com.c/.h`，采用与其他 abs 对象一致的 ops + handle，
但队列保存完整 USB payload，不解析 UART 帧头/帧尾。RX 为单生产者（USB IRQ）/
单消费者（主循环）的 C11 atomic 队列，无动态内存；调用方提供定长消息槽，
可以按业务选择槽容量，不必每个槽都分配 4096B。

可选的原生 USB 消息装配示例（当前 `g_usb` 保持 UART 帧适配）：

```c
static uint8_t usb_rx_slots[4U * (512U + 2U)];
static const tUsbBuffer usb_buf = {
    .rx_storage = usb_rx_slots,
    .rx_slot_count = 4U,
    .rx_payload_size = 512U,
};
static tUsbDriver usb_messages;
/* MX_USB_Device_Init() 仍由现有启动流程调用。 */
/* 检查 usb_init(&usb_messages, Get_USB_VendorHandle(), &usb_buf) 与 usb_start(&usb_messages) 返回值。 */
/* 主循环使用 usb_send() 和 usb_process_frame()。 */
```

`usb_process_frame()` 的 `tUsb_Frame.data/capacity` 由调用方设置；成功后
`data_len` 是完整 payload 长度。容量不足返回 false、给出所需长度且不出队。
队列满或报文超过槽容量时整条丢弃，累计 `rx_dropped`，不阻塞 USB 中断。
`usb_is_ready()` 反映枚举就绪，`usb_send()` 成功只代表 BSP 已拥有数据副本。
发送仅允许主循环/线程调用；BSP 拒绝 ISR 调用，避免打断 USB 回调中的队列更新。
运行中不得再次 init/reset abs 实例；USB 总线 reset 不清空 abs 已排队的完整消息，
业务若需要隔离连接会话，应在后续装配时增加相应的生命周期处理。

`usb_vendor_ops` 与旧 `uart_usb_ops` 使用同一 handle、同一个 RX callback 槽，
**不能同时启动两个接收实例**；后注册的接收者替代前者。切换到 `usb_com` 时应
停止调用原 `uart_start(&g_usb, ...)`，再注册原生 USB 实例。当前仍保留原装配，
因此新增 abs 接口还没有自动接管业务通信。

`usb_com` 不强制 payload 的业务格式；切换后可自行定义 cmd + data，无需套用
UART 帧，但必须同步修改主机和业务分发。本轮不执行这项业务迁移。
新 `.c` 由现有 abs `file(GLOB ...)` 在 CMake 配置时收录。

## 尚未涉及

不修改 Bootloader、`cmd_process.c`、上位机、任何 `.ioc` 或 CMake。
CubeMX 仍按 CustomHID 配置生成代码，重新生成后需人工恢复本目录改动。
现有 CMake 还存在指向旧 `USB_Device/...` 的路径，本轮不修正，不声明构建成功。
未验证枚举、吞吐、1kHz 时延、IAP 或热插拔行为。
