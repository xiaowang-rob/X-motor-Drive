#ifndef COMM_H
#define COMM_H

#include "bus_com.h"
#include "parameter_list.h"
#include "uart_com.h"
#include "usb_com.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PORT_UART,
    PORT_USB,
    PORT_CAN,
} ePort;

#define COMM_CAN_BLOCKS 16U
#define COMM_UART_RX_BYTES 256U
#define COMM_USB_RX_SLOTS 4U
#define COMM_USB_PAYLOAD_SIZE 128U

typedef struct {
    tBusDriver can;
    tUartDriver uart;
    tUsbDriver usb;
    _Alignas(tBlock) uint8_t can_pool[COMM_CAN_BLOCKS *
                                      (sizeof(tBlock *) + sizeof(tBus_Frame))];
    _Alignas(tBus_Frame *) uint8_t can_queue[COMM_CAN_BLOCKS * sizeof(tBus_Frame *)];
    uint8_t uart_rx[COMM_UART_RX_BYTES];
    uint8_t uart_frame[UART_MAX_PKT_SIZE + 4];
    uint8_t uart_tx[UART_MAX_PKT_SIZE + 5];
    uint8_t usb_rx[COMM_USB_RX_SLOTS * (COMM_USB_PAYLOAD_SIZE + 2U)];
} tComm;

/* 帧数据仅在回调执行期间有效。 */
typedef void (*tCommFrameHandler)(const void *context, ePort port, uint8_t cmd_id,
                                  const uint8_t *data, uint8_t len);

bool comm_init(tComm *comm);

bool comm_motor_start(tComm *comm, tParameter *param);

bool comm_bus_has_error(const tComm *comm);

bool comm_send_frame(tComm *comm, ePort port, uint8_t cmd_id, const uint8_t *data,
                     uint8_t len);

void comm_loop_task(tComm *comm, tCommFrameHandler on_frame,
                    const void *context);

#endif
