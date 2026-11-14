#include "obsdata.h"
#include "motor_ctrl.h"
#include "protection.h"

const tObsData g_data = {
    .current_u = &g_motor.fb.is[0],
    .current_v = &g_motor.fb.is[1],
    .current_w = &g_motor.fb.is[2],
    .voltage_q = &g_motor.fb.ualpha,
    .voltage_d = &g_motor.fb.ubeta,
    .current_alpha = &g_motor.fb.ialpha,
    .current_beta = &g_motor.fb.ibeta,
    .current_q = &g_motor.fb.uq,
    .current_d = &g_motor.fb.ud,
    .current_q_ref = &g_motor.ref.iq,
    .current_d_ref = &g_motor.ref.id,
    .velocity = &g_motor.fb.vel,
    .velocity_ref = &g_motor.ref.vel,
    .theta_elec = &g_motor.ref.theta_elec,
    .theta_mech = &g_motor.fb.theta_rotor,
    .position = &g_motor.fb.pos,
    .position_ref = &g_motor.ref.pos,
    /* STM32 为小端；协议状态字段为 U8，读取枚举的低字节。 */
    .motor_state = (uint8_t *)&g_motor.state,
    .fault = (uint8_t *)&pro_manager.fault,
    .vbus = &g_motor.fb.udc,
    .driver_temp = &g_motor.fb.temp,
    .motor_temp = &g_motor.fb.motor_temp,
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
