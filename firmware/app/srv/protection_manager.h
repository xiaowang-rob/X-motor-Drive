#ifndef __PROTECTION_MANAGER_H
#define __PROTECTION_MANAGER_H

#include "device.h"
#include "port_mapping.h"
#include "protocol.h"
#include "parameter_manager.h"
#include "foc_core.h"

typedef struct
{
    eFaultState fault;
    eWarningState warning;
    bool fault_flag;
    bool warning_flag;
    float max_current;
    float max_vel;
    float min_position;
    float max_position;
    float tolerance_time_ms;
    float tolerance_limit;

    tFOC_Mode *foc_mode;
    tFOC_val *foc_val;
    tCommunicationState *com_state;
    tDeviceStatus *drive_state;
} tProtectionManager;
extern tProtectionManager g_pro_manager;

// functions
void pro_manager_init(tParameter *param);
void pro_manager_config(tParameter *param);
void pro_manager_clear_flag();
void pro_manager_main_loop();

void pro_set_limit_position(float min_position, float max_position);

#endif // __PROTECTION_MANAGER_H