#ifndef MC_CONTROLLER_H
#define MC_CONTROLLER_H

#include "common/pid.h"
#include "common/mit.h"
#include "command/mc_command.h"
#include "core/mc_config.h"

typedef struct {
    tPID pos;
    tPI vel, id, iq, fw;
    tMIT mit;
    tMcMode mode;
    bool cascade_position, fw_enable;
    uint8_t pvc_remaining;
    float imax, inv_kt, q_limit, velocity_limit;
    float vref, qref;
    float current_ref, current_fb, duty_output;
    tDq iref, duty;
    tDq voltage_compensation_duty;
    float hfi_duty_d;
} tCtrl;

void ctrl_configure(tCtrl *c, const tParameter *p, const tMotorTiming *t,
                    const tCmdCfg *cfg);
void ctrl_measure(tCtrl *c, tDq current);
void ctrl_position_step(tCtrl *c, const tRef *ref, const tRotor *rotor);
void ctrl_velocity_step(tCtrl *c, const tRef *ref, const tRotor *rotor);
/* dq 占空比 1 对应 Vmax = Vbus/sqrt(3)。 */
tDq ctrl_step(tCtrl *c, const tRef *ref, tDq current);

#endif
