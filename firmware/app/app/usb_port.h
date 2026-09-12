#ifndef __USB_PORT_H
#define __USB_PORT_H

#include "bsp_usb.h"
#include "protocol.h"

typedef struct
{
    u8 head;
    u8 id;
    u8 len;
    u8 data[MAX_FRAME_LENGTH];
    u8 check;
    u8 tail;
} tUSB_Frame;

void usb_init(void);
bool usb_send_frame(u8 id, u8 *data, u8 len);
void usb_rx_frame_callback(u8 id, u8 *data, u8 len);

#endif // __USB_INTERFACE_H