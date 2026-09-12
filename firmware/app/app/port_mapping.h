#ifndef __PORT_MAPPING_H__
#define __PORT_MAPPING_H__

#include "protocol.h"
#include "device.h"
typedef enum
{
    NONE_port,
    CAN_port,
    UART_port,
    USB_port
} eCOM;

typedef struct
{
    bool is_busy;
    eCOM com_port;
    u8 cmd_id;
    u8 rxdatalen;
    u8 *rxdata;

    u8 txdatalen;
    u8 txdata[MAX_FRAME_LENGTH] __attribute__((aligned(4))); // 强制 4 字节对齐;

    u8 stream_num;
    eData_stream data_id_index[8];
} tCOM_Frame;

typedef struct
{
    eCOM Host_port;
    eCOM *com_port;
    bool *is_busy;
} tCommunicationState;
extern tCommunicationState g_com_state;

void comm_init();
void comm_write_can_config(u32 CAN_ID, bool canQUEUE);
void comm_main_loop();

#endif