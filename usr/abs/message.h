#ifndef __MESSAGE_H
#define __MESSAGE_H

#include "device.h"
#include "queue.h"

#define MESSAGE_UART_QUEUE_SIZE 256
#define MESSAGE_CAN_QUEUE_SIZE 256
#define MESSAGE_USB_QUEUE_SIZE 256
typedef enum
{
    MESSAGE_TYPE_UART,
    MESSAGE_TYPE_CAN,
    MESSAGE_TYPE_USB
} eMessageType;

typedef struct
{
    tStaticQueue queue;
    eDeviceStatus status;
    eMessageType type;
} tMessage;

message_init(tMessage *msg);
message_send(tMessage *msg, uint8_t id, uint8_t *data, uint16_t length);

#endif