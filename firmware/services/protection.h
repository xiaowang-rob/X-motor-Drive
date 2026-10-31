#ifndef __PROTECTION_H
#define __PROTECTION_H

#include "parameter_list.h"
#include "xfoc_main.h"

typedef struct {

    bool fault_flag;
    bool warning_flag;
    float max_current;
    float max_vel;
    float min_voltage;
    float max_voltage;
    float max_temp;
    float tolerance_time_ms;
} tProtectionManager;

// functions
void pro_manager_init(tParameter *param);
void pro_manager_reset(void);
void pro_manager_main_loop(tXFocCore *core);

#endif // __PROTECTION_H