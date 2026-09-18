#include "log.h"
#include "flash.h"

#include "bsp_base.h"
#include "device.h"
#include "string.h"
#include "math_fast.h"
#include "foc_main.h"

tLog log;
tFlashUnit log_unit; // 日志单元

static u8 num_tic = 0;

void log_data_save(tProtectionManager *pro_manager)
{
    log.num = num_tic;
    log.minutes = bsp_get_tick() / 1000 / 60;
    log.vbus = pro_manager->foc_val->udc;
    log.temp = pro_manager->foc_val->temp;
    log.iu = pro_manager->foc_val->iu;
    log.iv = pro_manager->foc_val->iv;
    log.iw = pro_manager->foc_val->iw;
    log.iq = pro_manager->foc_val->iq_fb;
    log.id = pro_manager->foc_val->id_fb;
    log.id_ref = pro_manager->foc_val->id_ref;
    log.iq_ref = pro_manager->foc_val->iq_ref;
    log.speed = pro_manager->foc_val->vel_fb;
    log.speed_ref = pro_manager->foc_val->vel_ref;
    log.position = pro_manager->foc_val->pos_fb;
    log.position_ref = pro_manager->foc_val->pos_ref;
    log.run_mode = pro_manager->foc_mode->run_mode;
    log.sensor_mode = pro_manager->foc_mode->sensor_mode;
    log.fault = (eFaultState)pro_manager->fault;
    log.warning = (eWarningState)pro_manager->warning;

    log.can_state = pro_manager->drive_state->can_state;
    log.encoder_state = pro_manager->drive_state->encoder_state;
}

void log_data_write(void)
{
    bsp_write_log((u8 *)&log, num_tic, sizeof(log));
    if (num_tic >= MAX_log_NUM)
    { // 日志满了后 循环覆盖最后一条日志
        return;
    }
    num_tic++;
}

static u8 read_index = 0;
bool log_read_flash(u8 *data, u8 *len)
{
    if (read_index < MAX_log_NUM)
    {
        bsp_read_log((u8 *)&log, read_index, sizeof(log));
        if (log.num == read_index)
        {
            *len = sizeof(log);
            read_index++;
            memcpy(data, &log, sizeof(log));
            return false;
        }
        else
        {
            read_index = 0;
            return true;
        }
    }
    else
    {
        *len = 0;
        read_index = 0;
        return true;
    }
}

bool log_erase(void)
{
    if (bsp_erase_log())
    {
        num_tic = 0;
        return true;
    }
    return false;
}