#ifndef MC_COMMAND_H
#define MC_COMMAND_H

#include "core/mc_types.h"
#include "common/trajectory.h"
#include "parameter_list.h"

typedef struct {
    tMcMode mode;
    bool fw;       /* 当前仅速度模式支持 */
    union {
        struct { float angle, vel, voltage; bool rotate; } openloop;
        float current;
        float torque;
        float vel;
        float pos;
        struct { float pos, vel, tau_ff, kp, kd; } mit;
    } value;
} tCmd;

typedef struct {
    tMcMode mode;
    bool rotate;
    tTraj traj;
    tRef ref;
} tCmdState;

tCmd cmd_default(tMcMode mode, const tParameter *param);
tMcResult cmd_validate(const tCmd *cmd, const tParameter *param);
void cmd_begin(tCmdState *state, const tCmd *cmd,
                      const tParameter *param, const tRotor *rotor);
void cmd_accept(tCmdState *state, const tCmd *cmd,
                       const tParameter *param);
bool cmd_update(tCmdState *state, const tParameter *param, float dt);

#endif
