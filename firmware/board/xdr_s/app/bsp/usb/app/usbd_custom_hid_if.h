#ifndef XDR_USBD_VENDOR_IF_H
#define XDR_USBD_VENDOR_IF_H

/* Historical filename retained for the existing build source list. Not HID. */
#include "usbd_core.h"

#define VENDOR_EPIN_ADDR  0x81U
#define VENDOR_EPOUT_ADDR 0x01U
#define VENDOR_EPIN_SIZE  64U
#define VENDOR_EPOUT_SIZE 64U

extern USBD_ClassTypeDef USBD_VENDOR;

/* Caller serializes these operations with the USB interrupt. Buffers must
 * remain valid until DataIn. A zero length transfer terminates a full packet. */
USBD_StatusTypeDef USBD_VENDOR_Transmit(const uint8_t *data, uint16_t len);
uint8_t USBD_VENDOR_IsReady(void);

#endif
