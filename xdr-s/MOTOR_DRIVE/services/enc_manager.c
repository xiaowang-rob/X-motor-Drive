#include "enc_manager.h"

#include "bsp_math.h"
#include "enc_abi.h"
#include "enc_as5047.h"
#include "enc_hall.h"
#include "enc_mt6816.h"
#include "enc_mt6835.h"
#include "enc_sincos.h"

// 外接编码器初始化
bool em_init(tEncManager *em, tParameter *param) {
    em->theta_offset = param->theta_offset;
    em->positive_drive = param->positive_dir ? 1 : -1;

    const tEncDevHandle *handle;
    tHallEncCfg hall_cfg;
    tAbiCfg abi_cfg;
    tSinCosEncCfg sincos_cfg;
    void *cfg = NULL;
    switch (param->rotor_enc) {
        case ENC_NONE:
            return true;
        case ENC_INT:
            handle = Get_EncIntHandle();
            break;
        case ENC_HALL:
            handle = Get_EncHallHandle();
            hall_cfg.sequence[0] = param->hall_table_0;
            hall_cfg.sequence[1] = param->hall_table_1;
            hall_cfg.sequence[2] = param->hall_table_2;
            hall_cfg.sequence[3] = param->hall_table_3;
            hall_cfg.sequence[4] = param->hall_table_4;
            hall_cfg.sequence[5] = param->hall_table_5;
            hall_cfg.prescaler = param->hall_psc;
            hall_cfg.filter = param->hall_filter;

            cfg = &hall_cfg;
            break;
        case ENC_SPI:
            switch (param->enc_chip) {
                case ENC_MT6816:
                    handle = Get_EncMT6816Handle();
                    break;
                case ENC_MT6835:
                    handle = Get_EncMT6835Handle();
                    break;
                case ENC_AS5047:
                    handle = Get_EncAS5047Handle();
                    break;
                default:
                    return false;
            }
            break;
        case ENC_ABI:
            handle = Get_EncAbiHandle();
            abi_cfg.enc_counts_per_rev = param->abi_count;
            abi_cfg.filter = param->abi_filter;
            abi_cfg.index_falling = param->abi_index;
            cfg = &abi_cfg;
            break;
        case ENC_SINCOS:
            handle = Get_EncSinCosHandle();
            sincos_cfg.sin_offset = param->sin_offset;
            sincos_cfg.cos_offset = param->cos_offset;
            sincos_cfg.sin_gain = param->sin_amp;
            sincos_cfg.cos_gain = param->cos_amp;
            sincos_cfg.filter_alpha = param->sincos_fl;
            cfg = &sincos_cfg;
            break;

        default:
            return false;
    }

    em->enc_active = enc_init(&em->enc, handle, cfg);
    return em->enc_active;
}

void em_loop_task(tEncManager *em) {
    if (!em->enc_active)
        return;
    // TODO:这里处理不同编码器角度
    enc_task(&em->enc);
    em->theta_r = em->enc.sample.angle_rad;
    em->theta_r = (em->theta_r - em->theta_offset) * em->positive_drive;
    // 这里的角度直接处理到 0-2pi
    em->theta_r=MATH_ANGLE_2PI(em->theta_r);
}
