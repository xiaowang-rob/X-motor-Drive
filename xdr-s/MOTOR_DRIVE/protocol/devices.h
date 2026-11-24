/* 此文件由 protocol/gen_protocol.py 自动生成，请勿手动修改 */
#ifndef PROTO_DEVICES_H
#define PROTO_DEVICES_H

#include <stdint.h>

#define VERSION_ENCODE(major, minor, patch) \
    ((((uint32_t)(major) & 0xFFU) << 16) |  \
     (((uint32_t)(minor) & 0xFFU) << 8)  |  \
     ((uint32_t)(patch) & 0xFFU))

#define XDR_S_APP_USB_VID 0x0483U
#define XDR_S_APP_USB_PID 0x5760U
#define XDR_S_BL_USB_VID 0x0483U
#define XDR_S_BL_USB_PID 0x5761U
#define XDR_P_APP_USB_VID 0x0483U
#define XDR_P_APP_USB_PID 0x5770U
#define XDR_P_BL_USB_VID 0x0483U
#define XDR_P_BL_USB_PID 0x5771U
#endif
