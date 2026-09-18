#include "foc.h"
#include "filter.h"
#include "slot_con.h"

// FOC核心初始化
bool foc_init(tFOC *foc, tParameter *param, float t_cl, float vmax)
{
    pi_init(&foc->PI_iq, param->qclkp, param->qclki, vmax, t_cl);
    pi_init(&foc->PI_id, param->dclkp, param->dclki, vmax, t_cl);

    filter_first_order_lag_init(&foc->cf_u, param->cfalpha, 0);
    filter_first_order_lag_init(&foc->cf_v, param->cfalpha, 0);
    filter_first_order_lag_init(&foc->cf_w, param->cfalpha, 0);

    foc_reset(foc);
}

bool foc_reset(tFOC *foc)
{
    pi_reset(&foc->PI_iq);
    pi_reset(&foc->PI_id);
    filter_first_order_lag_reset(&foc->cf_u, 0);
    filter_first_order_lag_reset(&foc->cf_v, 0);
    filter_first_order_lag_reset(&foc->cf_w, 0);

    memset(&foc->tag, 0, sizeof(tFOCtarget));
    memset(&foc->val, 0, sizeof(tFOCval));
}

// 电角度预计算、原始三相电流处理 重构-滤波-clark-park
void foc_process(tFOC *foc, uint8_t sec)
{

    // 预计算sin cos
    arm_sin_cos_rad_f32(foc->val.theta_elec, &foc->val.sin_e, &foc->val.cos_e);
    // 根据扇区确定两相：最短导通相由另两相推导 (Ia+Ib+Ic=0)
    if (sec == 1 || sec == 6)
    { // 最短相=W
        foc->val.iu = foc->val.imv + foc->val.imw;
        foc->val.iv = -foc->val.imv;
        foc->val.iw = -foc->val.imw;
    }
    else if (sec == 2 || sec == 3)
    { // 最短相=U
        foc->val.iu = -foc->val.imu;
        foc->val.iv = foc->val.imu + foc->val.imw;
        foc->val.iw = -foc->val.imw;
    }
    else if (sec == 4 || sec == 5)
    { // 最短相=V
        foc->val.iu = -foc->val.imu;
        foc->val.iv = -foc->val.imv;
        foc->val.iw = foc->val.imu + foc->val.imv;
    }
    else
    { // sec 0/7: 零矢量
        foc->val.iu = foc->val.imu;
        foc->val.iv = foc->val.imv;
        foc->val.iw = foc->val.imw;
    }

    foc->val.iu = filter_first_order_lag(&foc->cf_u, foc->val.iu);
    foc->val.iv = filter_first_order_lag(&foc->cf_v, foc->val.iv);
    foc->val.iw = filter_first_order_lag(&foc->cf_w, foc->val.iw);
    // Clarke 变换
    clarke_transform(foc->val.iu, foc->val.iv, foc->val.iw, &foc->val.ialpha, &foc->val.ibeta);
    // Park 变换
    park_transform(foc->val.ialpha, foc->val.ibeta, foc->val.sin_e, foc->val.cos_e, &foc->val.id, &foc->val.iq);
}
// 电流内环更新 pi输出dq电压 反park变换为ab电压 再融合HFI
void foc_update(tFOC *foc)
{
    foc->val.uq = pi_update(&foc->PI_iq, foc->tag.iq, foc->val.iq);
    foc->val.ud = pi_update(&foc->PI_id, foc->tag.id, foc->val.id);

    inv_park_transform(foc->val.ud, foc->val.uq, foc->val.sin_e, foc->val.cos_e,
                       &foc->val.ualpha, &foc->val.ubeta);
}

// 设置各环指令值
void foc_set_target(tFOC *foc, tFOCtarget tag)
{
    foc->tag = tag;
}
