# 此文件由 protocol/gen_protocol.py 自动生成，请勿手动修改

def VERSION_ENCODE(major, minor, patch):
    return ((major & 0xFF) << 16) | ((minor & 0xFF) << 8) | (patch & 0xFF)

XDR_S_APP_USB_VID = 0x0483
XDR_S_APP_USB_PID = 0x5760
XDR_S_BL_USB_VID = 0x0483
XDR_S_BL_USB_PID = 0x5761
XDR_P_APP_USB_VID = 0x0483
XDR_P_APP_USB_PID = 0x5770
XDR_P_BL_USB_VID = 0x0483
XDR_P_BL_USB_PID = 0x5771
