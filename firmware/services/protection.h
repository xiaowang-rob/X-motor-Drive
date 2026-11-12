#ifndef __PROTECTION_H
#define __PROTECTION_H

#include "parameter_list.h"
#include "xfoc_main.h"

typedef struct {
    eFault fault; // 故障
    float max_current;
    float max_vel;
    float min_voltage;
    float max_voltage;
    float max_temp;
    float tolerance_time_ms;
} tProtectionManager;

extern tProtectionManager pro_manager;

// functions
void pro_manager_init(tParameter *param);
void pro_manager_reset(void);
void pm_handle_fault(eFault fault);
void pm_loop_task(tMotorCtrl *motor);

#endif // __PROTECTION_H