#include "comm.h"

#include "bsp_bus.h"
#include "bsp_uart.h"
#include "bsp_usb_vendor.h"

#include <string.h>

bool comm_send_frame(tComm *comm, ePort port, uint8_t cmd_id, const uint8_t *data,
                     uint8_t len) {
    if (port == PORT_CAN) {
        tBus_Frame frame;
        frame.id = cmd_id;
        frame.data_len =
                (len > sizeof(frame.data)) ? (uint8_t) sizeof(frame.data) : len;
        if (frame.data_len && data)
            memcpy(frame.data, data, frame.data_len);
        return bus_send(&comm->can, &frame);
    } else if (port == PORT_UART) {
        tUart_Frame frame;
        frame.id = cmd_id;
        frame.data_len = len;
        if (len && data)
            memcpy(frame.data, data, len);
        return uart_send(&comm->uart, &frame);
    } else {
        return usb_send(&comm->usb, cmd_id, data, len);
    }
}

// 驱动器通讯初始化 USB UART
bool comm_init(tComm *comm) {
    // usb 初始化+启动
    const tUsbBuffer usb_buf = {
        .rx_storage = comm->usb_rx,
        .rx_slot_count = COMM_USB_RX_SLOTS,
        .rx_payload_size = COMM_USB_PAYLOAD_SIZE,
    };
    if (!comm->usb.initialized &&
        !usb_init(&comm->usb, Get_USB_VendorHandle(), &usb_buf) &&
        !usb_start(&comm->usb))
        return false;

    // uart 初始化+启动
    const tUartBuffer buf = {
        .rx_queue_buf = comm->uart_rx,
        .rx_queue_size = sizeof(comm->uart_rx),
        .frame_buf = comm->uart_frame,
        .tx_buf = comm->uart_tx,
    };
    if (!uart_init(&comm->uart, Get_MCU_UsartHandle(), &buf) &&
        !uart_start(&comm->uart))
        return false;
    // 初始化总线通讯
    const tBusBuffer can_buf = {
        .mp_buf = comm->can_pool,
        .mp_block_num = COMM_CAN_BLOCKS,
        .queue_buf = comm->can_queue,
        .queue_bytes = sizeof(comm->can_queue),
    };
    if (!bus_init(&comm->can, Get_CanHandle(), &can_buf))return false;

    return true;
}


// 电机控制通讯启动 CAN 485
bool comm_motor_start(tComm *comm, tParameter *param) {
    return bus_start(&comm->can, param->can_id);
}

bool comm_bus_has_error(const tComm *comm) {
    return comm->can.dstate == DEV_RUN_ERROR;
}

void comm_loop_task(tComm *comm, tCommFrameHandler on_frame,
                    const void *context) {
    tBus_Frame bus_frame;
    while (bus_process_frame(&comm->can, &bus_frame)) {
        if (bus_frame.data_len == 0U)
            continue;
        on_frame(context, PORT_CAN, bus_frame.data[0], &bus_frame.data[1],
                 (uint8_t) (bus_frame.data_len - 1U));
    }

    tUart_Frame uart_frame;
    while (uart_process_frame(&comm->uart, &uart_frame)) {
        on_frame(context, PORT_UART, uart_frame.id, uart_frame.data,
                 uart_frame.data_len);
    }

    uint8_t usb_data[COMM_USB_PAYLOAD_SIZE];

    tUsb_Frame usb_frame = {
        .data = usb_data,
        .capacity = sizeof(usb_data),
    };

    while (usb_process_frame(&comm->usb, &usb_frame)) {
        on_frame(context, PORT_USB, usb_frame.id, usb_frame.data,
                 usb_frame.data_len);
    }
}
