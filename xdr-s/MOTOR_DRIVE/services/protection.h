#ifndef __PROTECTION_H
#define __PROTECTION_H

#include "led.h"
#include "storage.h"
#include "mc_motor.h"
#include "parameter_list.h"
#include "data_list.h"

typedef enum {
    STATE_IDLE,
    STATE_FAULT,
    STATE_RUNNING,
} eStates;

typedef struct {
    tLed led;
    tStorage log_storage;
    bool led_ready;
    eStates state;
    eFault fault; // 故障
    float max_current;
    float max_vel;
    float min_voltage;
    float max_voltage;
    float max_temp;
    float tolerance_time_ms;
} tProtectionManager;

void PM_error_Callback(tProtectionManager *pm);

// functions
bool protection_init(tProtectionManager *pm);

void protection_config(tProtectionManager *pm, tParameter *param);

void protection_reset(tProtectionManager *pm, tMotorCtrl *motor);

void protection_fault(tProtectionManager *pm, eFault fault);

void protection_loop_task(tProtectionManager *pm, tMotorCtrl *motor,
                          const tObsData *data);

#endif // __PROTECTION_H
