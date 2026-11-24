#include "mc_command.h"

tCmdCfg cmd_default(tMcMode mode, const tParameter *p) {
    tCmdCfg cfg = {.mode = mode};
    if (mode == MC_MODE_MIT) {
        cfg.config.mit.kp = p->mit_kp;
        cfg.config.mit.kd = p->mit_kd;
    }
    return cfg;
}

#ifndef NDEBUG
bool cmd_config_valid(const tCmdCfg *cmd, const tParameter *p) {
    if (!mc_mode_is_active(cmd->mode))
        return false;
    if (cmd->mode == MC_MODE_OPENLOOP)
        return true;
    if (p->sensorless != SENSORLESS_NONE || p->rotor_enc == ENC_NONE)
        return false;
    if (cmd->mode == MC_MODE_MIT ||
        (cmd->mode == MC_MODE_CURRENT && cmd->config.torque_control))
        return p->motor_ke > 0.0f;
    return true;
}
#endif
