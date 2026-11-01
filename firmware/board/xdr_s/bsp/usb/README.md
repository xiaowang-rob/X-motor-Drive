# XDr-S USB Vendor Bulk

## 设备与端点

- VID/PID 通过 APP、BL 各自的 `bsp_cfg.h` 引用协议设备定义。
- Vendor 接口 0、alternate setting 0，class `0xFF`。
- OUT `0x01`，IN `0x81`，Full Speed Bulk，最大 packet 64 字节。
- Windows 可用 Zadig 绑定 WinUSB，上位机通过 libusb/PyUSB 访问。

## 线上帧格式

```text
offset  bytes  字段
0       2      A5 5A
2       1      command
3       2      数据长度 N，小端，0..4095
5       N      数据
5+N     2      CRC16，小端
```

数据长度不包含命令 ID、帧头、长度字段和 CRC。无参数命令的 N 为 0，
最短帧 7 字节。已移除 version、flags 和 seq，与旧格式不兼容。

CRC16/CCITT-FALSE：poly `0x1021`、init `0xFFFF`、refin/refout=false、xorout=0。
覆盖整个 5 字节头部和数据，不包含 CRC 字段。总长度为 N + 7。

USB packet 边界不是消息边界；双方必须按长度重组消息，并支持半帧和连续多帧。
设备发送以短包结束一条消息；线长度恰为 64 的倍数时追加 ZLP。
RX 无新字节 1000ms 后处理半包超时，非法长度或 CRC 使用逐字节重新同步。
协议没有自动重传和请求序号，响应匹配、超时及重试策略由业务层负责。

## 固件内部接口

`usb_send()` 和 RX 回调使用 `[command:u8][data...]`，总长 1..4096。
BSP 将首字节命令放进线头，接收时再恢复内部消息，不嵌套 UART 帧。
`usb_com` 队列保存完整内部消息；`usb_process_frame()` 解出 `tUsb_Frame.id`、
`data`、`data_len`，其中 `data` 和 `data_len` 只表示参数，不包含命令 ID。
无参数命令仍成功出队，允许 `data = NULL`、`capacity = 0`。
参数缓冲不足时返回 false，给出 ID 和所需参数长度，保留消息以供重试。
实例槽容量包含命令 ID，调用方接收缓冲只需容纳参数。
BL 当前使用 129 字节消息缓冲，WRITE 数据块最大 124 字节。

发送返回 true 只表示数据已复制入队，不代表送达或业务确认。
未配置、队列满、并发发送或 ISR 发送返回 false，调用方负责稍后重试。
USB reset/重新配置清空 BSP 收发队列，保留回调和累计统计；
`usb_com` 已排队的完整消息不会随总线 reset 自动清空。

RX 回调在 USB 中断上下文执行，必须立即复制/入队，不能阻塞。
业务应在主循环调用 `usb_process_frame()` 后处理。

上位机目前只有 USB 设备发现，尚未接通 Bulk 传输；APP 业务收发也尚未接通。
修改协议不等于这些功能已实现。CubeMX 重新生成 USB 类代码后仍需检查 Vendor 配置。
尚未进行本轮实机枚举、吞吐、热插拔和 IAP 验证。
