#ifndef MC_MOTOR_H
#define MC_MOTOR_H

#include "core/mc_config.h"
#include "control/mc_motion.h"
#include "control/mc_controller.h"
#include "control/mc_pwm.h"
#include "sensor/mc_sensor.h"

typedef struct {
    /* 服务层负责采样与 PWM 同步，以及反馈监管。回调不可重入 motor。 */
    void (*read_sample)(tMcSample *sample);
    void (*write_pwm)(const tPwm *pwm);
    void (*set_enable)(bool enabled);
} tMotorOps;

/* 应用持有实例，可直接读取反馈。跨中断读取同一拍的多个字段时使用临界区。
 * 参数通过 motor_config() 应用，以同步重建控制器和传感器状态。 */
typedef struct {
    tParameter param;             /* 当前协议参数快照 */
    tMotorTiming timing;
    tMotorOps port;
    tMcMode mode;
    tCmdCfg pending_config;
    tCmdValue pending_value;
    bool value_pending;
    tMcMotion motion;
    tCtrl ctrl;
    tMcSensor sensor;
    tPwm pwm;                     /* 实际提交到硬件的 PWM */
    uint16_t medium_slot;
    float vbus;
} tMotorCtrl;

/* 所有 API 以有效指针、已完成初始化、有限输入和正确端口为调用前提。
 * init 只调用一次或在 tick 完全停止、输出已关闭后调用。 */
void motor_init(tMotorCtrl *motor, const tParameter *param,
                        const tMotorTiming *timing, const tMotorOps *port);
/* IDLE 时应用参数。服务层负责配置和指令有效性。 */
void motor_config(tMotorCtrl *motor, const tParameter *param);
/* IDLE 时提交配置，配置时隙完成后开始运行。 */
void motor_cmd_cfg(tMotorCtrl *motor, const tCmdCfg *cfg);
/* 提交当前模式的新目标值，在下一个运动时隙消费。 */
void motor_cmd_set(tMotorCtrl *motor, const tCmdValue *cmd);
/* 服务层调用此接口立即关断 PWM。 */
void motor_stop(tMotorCtrl *motor);
void motor_zero(tMotorCtrl *motor);
/* 每 dt 调用一次，单一周期入口，不得并发/重入。 */
void motor_tick(tMotorCtrl *motor);

#endif
