#ifndef __PROTECTION_H
#define __PROTECTION_H

#include "parameters.h"
#include "protocol.h"

typedef struct
{
    eFault fault;
    eWarning warning;
    bool fault_flag;
    bool warning_flag;
    float max_current;
    float max_vel;
    float min_position;
    float max_position;
    float tolerance_time_ms;
    float tolerance_limit;
} tProtectionManager;
extern tProtectionManager g_pro_manager;

// functions
void pro_manager_init(tParameter *param);
void pro_manager_clear_flag();
void pro_manager_main_loop();

void pro_set_limit_position(float min_position, float max_position);

#endif // __PROTECTION_H