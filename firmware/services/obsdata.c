#include "obsdata.h"
#include "protection.h"
#include "xfoc_main.h"

const tObsData g_data = {
    .current_u = &motor.fb.is[0],
    .current_v = &motor.fb.is[1],
    .current_w = &motor.fb.is[2],
    .voltage_q = &motor.fb.uq,
    .voltage_d = &motor.fb.ud,
    .current_alpha = &motor.fb.ialpha,
    .current_beta = &motor.fb.ibeta,
    .current_q = &motor.fb.iq,
    .current_d = &motor.fb.id,
    .current_q_ref = &motor.ref.iq,
    .current_d_ref = &motor.ref.id,
    .velocity = &motor.fb.vel,
    .velocity_ref = &motor.ref.vel,
    .theta_elec = &motor.ref.theta_elec,
    .theta_mech = &motor.fb.theta_rotor,
    .position = &motor.fb.pos,
    .position_ref = &motor.ref.pos,
    /* STM32 为小端；协议状态字段为 U8，读取枚举的低字节。 */
    .motor_state = (uint8_t *)&motor.state,
    .fault = (uint8_t *)&pro_manager.fault,
    .vbus = &motor.fb.udc,
    .driver_temp = &motor.fb.temp,
    .motor_temp = &motor.fb.motor_temp,
};

static tDataField g_data_slots[DATA_NUM];

void dm_data_init(void) {
#define D_SLOT(id, field, type)                                                \
    g_data_slots[id] = (tDataField){g_data.field, type};

    DATA_FIELDS(D_SLOT)

#undef D_SLOT
}

// 读一个通道：解引用绑定地址；未绑定返回 0.0f
void dm_data_get(eObsData data, uint8_t *val, uint8_t *len) {
    if ((uint16_t)data >= DATA_NUM || !len) {
        *val = 0.0f;
        *len = 0;
        return;
    }

    *len = field_value_type_size(g_data_slots[data].type);
    memcpy(val, g_data_slots[data].ptr, *len);
}
