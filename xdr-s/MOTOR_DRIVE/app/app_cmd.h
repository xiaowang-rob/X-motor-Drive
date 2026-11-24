#ifndef APP_CMD_H
#define APP_CMD_H

#include "comm.h"
#include "enc_manager.h"
#include "obsdata.h"
#include "parameters.h"
#include "protection.h"
#include "tune.h"

typedef struct {
    tComm *comm;
    tEncManager *enc;
    tObsDataService *obs;
    tParamService *param;
    tProtectionManager *protection;
    tMotorCtrl *motor;
    tTune *tune;
} tAppCmdServices;

void app_cmd_loop_task(const tAppCmdServices *services);

#endif
