#ifndef __LOG_H
#define __LOG_H

#include "bsp_cfg.h"
#include "flash.h"
#include "protection.h"

#define MAX_log_NUM 9

typedef struct
{
    uint8_t num;
    uint8_t minutes;
    uint8_t fault;
    uint8_t warning;

    uint8_t sensor_mode;
    uint8_t run_mode;

    uint8_t can_state;
    uint8_t encoder_state;

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
bool log_read_flash(uint8_t *data, uint8_t *len);
bool log_erase(void);

#endif // __LOG_H