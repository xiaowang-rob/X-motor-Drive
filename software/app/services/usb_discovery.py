"""USB 设备枚举；ID 来自 protocol/devices.json 的生成结果。"""
from protocol import devices

try:
    import usb.core as usb_core
except ImportError:
    usb_core = None


USB_DEVICES = {
    (getattr(devices, f"{board}_{mode}_USB_VID"),
     getattr(devices, f"{board}_{mode}_USB_PID")): f"{name} {mode}"
    for board, name in (("XDR_S", "XDr-S"), ("XDR_P", "XDr-P"))
    for mode in ("APP", "BL")
}


def scan_usb_devices():
    """返回界面列表条目；不打开设备、不认领接口或读取字符串描述符。"""
    if usb_core is None:
        raise RuntimeError("未安装 PyUSB，请安装 software/requirements.txt 中的依赖")
    entries = []
    for device in usb_core.find(
        find_all=True,
        custom_match=lambda dev: (dev.idVendor, dev.idProduct) in USB_DEVICES,
    ):
        vid, pid = device.idVendor, device.idProduct
        name = USB_DEVICES[(vid, pid)]
        location = f"{device.bus}:{device.address}"
        entries.append({
            "device": f"usb:{vid:04x}:{pid:04x}:{location}",
            "transport": "usb",
            "type": name,
            "vid": vid,
            "pid": pid,
            "display": f"(USB){name.removesuffix(' APP')}",
        })
    return sorted(entries, key=lambda entry: entry["device"])
