#include "app_main.h"
#include "app_cmd.h"

#include "bsp_config.h"
#include "bsp_mcu.h"
#include "gate_drv.h"

static tMotorCtrl motor;
static tProtectionManager pro_manager;
static tParamService param;
static tGateDrv gate_drv;
static tEncManager enc_manager;
static tComm comm;
static tObsDataService obs_service;
static tTune tune;

static const tAppCmdServices cmd_services = {
    .comm = &comm,
    .enc = &enc_manager,
    .obs = &obs_service,
    .param = &param,
    .protection = &pro_manager,
    .motor = &motor,
    .tune = &tune,
};

static void motor_read_sample(tMcSample *sample);

static void motor_enable(bool enable);

static void motor_set_pwm(const tPwm *pwm);

const static tMotorOps motor_ops = {
    .read_sample = motor_read_sample,
    .set_enable = motor_enable,
    .write_pwm = motor_set_pwm,
};

const static tObsData obs_data = {
    .current_u = &motor.sensor.is[0],
    .current_v = &motor.sensor.is[1],
    .current_w = &motor.sensor.is[2],
    .current_d = &motor.sensor.i_dq.d,
    .current_q = &motor.sensor.i_dq.q,
    .current_d_ref = &motor.ctrl.iref.d,
    .current_q_ref = &motor.ctrl.iref.q,
    .current_fb = &motor.ctrl.current_fb,
    .current_ref = &motor.ctrl.current_ref,
    .duty = &motor.ctrl.duty_output,
    .velocity = &motor.sensor.rotor.vel,
    .velocity_ref = &motor.ctrl.vref,
    .theta_elec = &motor.sensor.rotor.theta_e,
    .theta_mech = &motor.sensor.rotor.theta,
    .position = &motor.sensor.rotor.pos,
    .position_ref = &motor.motion.ref.pos,
    .motor_state = (uint8_t *) &motor.mode,
    .fault = (uint8_t *) &pro_manager.fault,
    .vbus = &gate_drv.Vbus,
    .driver_temp = &gate_drv.Temp,
};

void PM_error_Callback(tProtectionManager *pm) {
    //   B错误处理--日志模块还得优化

    // log_data_save(&pro_manager);
    //  TODO: 添加错误处理
    //   log_data_write();
    //  TODO: 添加警告处理
    //   log_data_write();
}

bool app_main_init(void) {
    bsp_time_init();

    // 如果是需要初始化的存储器 需要提前初始化

    // 初始化参数服务+读参数
    if (!param_init(&param))
        return false;
    // 初始化保护服务
    if (!protection_init(&pro_manager))
        return false;
    // 初始化观测服务 挂钩观测变量地址
    dm_data_init(&obs_service, &obs_data);
    // 驱动通讯初始化
    if (!comm_init(&comm))
        return false;
    // 栅极驱动模块初始化
    gd_init(&gate_drv);

    return true;
}

void app_param_apply(void) {
    // 配置保护服务
    protection_config(&pro_manager, &param.value);
    // 配置电机控制总线
    if (!comm_motor_start(&comm, &param.value))
        protection_fault(&pro_manager, FAULT_BUSCOM_INIT);
    // 编码器初始化
    if (!em_init(&enc_manager, &param.value))
        protection_fault(&pro_manager, FAULT_ENCODER_INIT);
    // 电机算法初始化
    tMotorTiming motor_timing = {
        .dt = T_PWM,
        .medium_div = 10,
    };
    motor_init(&motor, &param.value, &motor_timing, &motor_ops);
}

void app_main_start(void) {
    app_param_apply();
    while (1) {
        // 通讯
        app_cmd_loop_task(&cmd_services);
        // 编码器任务
        em_loop_task(&enc_manager);
        //  栅极驱动任务
        gd_loop_task(&gate_drv);
        // 保护
        protection_loop_task(&pro_manager, &motor, &obs_data);
    }
}
