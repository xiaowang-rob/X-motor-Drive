#include "log.h"
#include "bsp_base.h"
#include "device.h"
#include "string.h"
#include "math_fast.h"
#include "foc_main.h"

tLog Log;
static u8 num_tic = 0;

void log_data_save(tProtectionManager *pro_manager)
{
    Log.num = num_tic;
    Log.minutes = bsp_get_tick() / 1000 / 60;
    Log.vbus = pro_manager->foc_val->udc;
    Log.temp = pro_manager->foc_val->temp;
    Log.iu = pro_manager->foc_val->iu;
    Log.iv = pro_manager->foc_val->iv;
    Log.iw = pro_manager->foc_val->iw;
    Log.iq = pro_manager->foc_val->iq_fb;
    Log.id = pro_manager->foc_val->id_fb;
    Log.id_ref = pro_manager->foc_val->id_ref;
    Log.iq_ref = pro_manager->foc_val->iq_ref;
    Log.speed = pro_manager->foc_val->vel_fb;
    Log.speed_ref = pro_manager->foc_val->vel_ref;
    Log.position = pro_manager->foc_val->pos_fb;
    Log.position_ref = pro_manager->foc_val->pos_ref;
    Log.run_mode = pro_manager->foc_mode->run_mode;
    Log.sensor_mode = pro_manager->foc_mode->sensor_mode;
    Log.fault = (eFaultState)pro_manager->fault;
    Log.warning = (eWarningState)pro_manager->warning;

    Log.can_state = pro_manager->drive_state->can_state;
    Log.encoder_state = pro_manager->drive_state->encoder_state;
}

void log_data_write(void)
{
    bsp_write_log((u8 *)&Log, num_tic, sizeof(Log));
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
        bsp_read_log((u8 *)&Log, read_index, sizeof(Log));
        if (Log.num == read_index)
        {
            *len = sizeof(Log);
            read_index++;
            memcpy(data, &Log, sizeof(Log));
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