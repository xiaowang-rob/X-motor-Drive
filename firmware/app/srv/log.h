#ifndef __LOG_H
#define __LOG_H

#include "bsp_base.h"
#include "bsp_flash.h"
#include "protection_manager.h"

#define MAX_log_NUM 9

typedef struct
{
    u8 num;
    u8 minutes;
    u8 fault;
    u8 warning;

    u8 sensor_mode;
    u8 run_mode;

    u8 can_state;
    u8 encoder_state;

    float vbus;
    float temp;
    float iu;
    float iv;
    float iw;
    float id;
    float iq;
    float id_ref;
    float iq_ref;
    float speed;
    float speed_ref;
    float position;
    float position_ref;
} tLog;

void log_data_save(tProtectionManager *pro_manager);
void log_data_write(void);
bool log_read_flash(u8 *data, u8 *len);
bool log_erase(void);

#endif // __LOG_H