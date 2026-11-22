#include "obsdata.h"
#include "gate_drv.h"
#include "mc_motor.h"
#include "protection.h"

const tObsData g_data = {
    .current_u = &g_motor.foc.fb.is[0],
    .current_v = &g_motor.foc.fb.is[1],
    .current_w = &g_motor.foc.fb.is[2],
    .current_d = &g_motor.foc.fb.id,
    .current_q = &g_motor.foc.fb.iq,
    .current_d_ref = &g_motor.foc.ref.id,
    .current_q_ref = &g_motor.foc.ref.iq,
    .duty = &g_motor.foc.out.duty,
    .velocity = &g_motor.ctrl.fb.vel,
    .velocity_ref = &g_motor.ctrl.ref.vel,
    .theta_elec = &g_motor.foc.ref.theta_e,
    .theta_mech = &g_motor.sen.theta_r,
    .position = &g_motor.ctrl.fb.pos,
    .position_ref = &g_motor.ctrl.ref.pos,
    .motor_state = (uint8_t *)&g_motor.state,
    .fault = (uint8_t *)&pro_manager.fault,
    .vbus = &g_gate_drv.Vbus,
    .driver_temp = &g_gate_drv.Temp,
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
