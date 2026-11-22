#ifndef MC_MOTOR_H
#define MC_MOTOR_H

#include "core/mc_config.h"
#include "command/mc_command.h"
#include "control/mc_controller.h"
#include "control/mc_foc.h"
#include "control/mc_sensor.h"

typedef struct {
    void *user;
    /* 采样须对应 applied；填入有效相掩码和 PWM 序号。额外硬件流水延迟
     * 由适配层保存实际应用的元数据处理，不能把新 PWM 当作旧采样来源。 */
    bool (*read_sample)(void *user, const tPwm *applied, tSample *sample);
    void (*write_pwm)(void *user, const tPwm *pwm);
    void (*set_enable)(void *user, bool enabled);
    bool (*get_temp)(void *user, float *temperature);
    /* 成对提供，token 保存/恢复原中断状态。回调不可重入 motor。
     * 都为空时，所有 API（含故障接口）只能串行调用。 */
    uintptr_t (*enter_critical)(void *user);
    void (*exit_critical)(void *user, uintptr_t token);
} tMotorOps;

/* 固定大小、调用者所有。运行期间仅通过 API 访问，不直接写内部字段。 */
typedef struct {
    tParameter param;             /* 当前协议参数快照 */
    tMotorTiming timing;
    float cmd_dt;
    tMotorOps port;
    tMcRunState state;
    uint32_t faults;
    bool ready;
    tCmd mailbox;
    bool cmd_pending;
    uint32_t cmd_seq;
    tCmdState cmd;
    tCtrl ctrl;
    tMcFoc foc;
    tMcSensor sensor;
    tPwm pwm;                     /* 实际提交到硬件的 PWM */
    uint16_t cmd_tick, vel_tick, pos_tick, slow_tick;
    uint32_t traj_seq;             /* 前台规划提交时检测轨迹是否前进 */
    float vbus, temp;
    bool temp_valid;
} tMotorCtrl;

typedef struct {
    tMcRunState state;
    tMcMode mode;
    uint32_t faults;
    tRotor rotor;
    tRef ref;
    tDq current, iref;
    float is[3];
    float voltage_utilization, vref;
    tAb applied_voltage;
    tPwm pwm;
    float vbus, temp;
    bool temp_valid;
} tMotorFb;

/* 所有 API 以有效指针、已完成初始化、有限输入和正确端口为调用前提。
 * init 只调用一次或在 tick 完全停止、输出已关闭后调用。 */
tMcResult motor_init(tMotorCtrl *motor, const tParameter *param,
                        const tMotorTiming *timing, const tMotorOps *port);
/* 只在 IDLE 应用，保留端口/周期，重建算法状态并重新等待有效采样。 */
tMcResult motor_config(tMotorCtrl *motor, const tParameter *param);
tMcResult motor_submit(tMotorCtrl *motor, const tCmd *cmd);
tMcResult motor_start(tMotorCtrl *motor);
/* 立即关断并进入 IDLE（有锁存故障时仍为 FAULT）；下次 start 复位控制器。 */
void motor_stop(tMotorCtrl *motor);
void motor_fault(tMotorCtrl *motor, uint32_t faults);
tMcResult motor_clear_fault(tMotorCtrl *motor);
tMcResult motor_zero(tMotorCtrl *motor);
/* 每 dt 调用一次，单一周期入口，不得并发/重入。 */
void motor_tick(tMotorCtrl *motor);
/* 前台循环调用；S 型新目标需要本函数规划，tick 不执行迭代搜索。
 * service 不可重入，可被 tick 中断；规划过期时返回 MC_BUSY，稍后重试。 */
tMcResult motor_service(tMotorCtrl *motor);
tMcResult motor_get_fb(tMotorCtrl *motor, tMotorFb *feedback);

#endif
