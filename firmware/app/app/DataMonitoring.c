#include "DataMonitoring.h"
#include "foc_main.h"
#include "svpwm.h"
#include "string.h"

#include "hfi.h"

static float temp_val = 0;

static u8 txdata_queue[64];

void stream_data_get(eData_stream stream, float *data)
{
    tFOC_val *foc_val = get_foc_val_adr();
    switch (stream)
    {
    case CURRENT_U:
        //        temp_val = get_voltage_u();
        //  temp_val = foc_val->iu;
        temp_val = g_hfi.vel_filtered;
        memcpy(data, &temp_val, 4);
        break;
    case CURRENT_V:
        //        temp_val = get_voltage_v();
        temp_val = foc_val->iv;
        memcpy(data, &temp_val, 4);
        break;
    case CURRENT_W:
        //        temp_val = get_voltage_w();
        temp_val = foc_val->iw;
        memcpy(data, &temp_val, 4);
        break;
    case VOLTAGE_Q:
        *data = foc_val->uq;
        break;
    case VOLTAGE_D:
        *data = foc_val->ud;
        break;
    case CURRENT_ALPHA:
        *data = foc_val->ialpha;
        break;
    case CURRENT_BETA:
        *data = foc_val->ibeta;
        break;
    case CURRENT_Q:
        *data = foc_val->iq_fb;
        break;
    case CURRENT_D:
        *data = foc_val->id_fb;
        break;
    case CURRENT_Q_REF:
        *data = foc_val->iq_ref;
        break;
    case CURRENT_D_REF:
        *data = foc_val->id_ref;
        break;
    case SPEED:
        *data = foc_val->vel_fb;
        break;
    case SPEED_REF:
        *data = foc_val->vel_ref;
        break;
    case THETA_ELEC:
        *data = foc_val->theta_elec;
        break;
    case THETA_MECH:
        *data = foc_val->theta_mech;
        break;
    case POSITION:
        *data = foc_val->pos_fb;
        break;
    case POSITION_REF:
        *data = foc_val->pos_ref;
        break;
    default:
        break;
    }
}

void stream_data_prepare(eData_stream stream, u8 index, u8 *data, bool _tx)
{
    stream_data_get(stream, (float *)&txdata_queue[index * 4]);
    if (_tx)
    {
        memcpy(data, txdata_queue, (index + 1) * 4);
    }
}
