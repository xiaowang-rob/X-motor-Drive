// ============================================================
// gate_fd6288q.c — 电机功率级驱动（板级，直连 HAL）
//
// TIM8 中心对齐 6 路 PWM（CH1-3 + 互补）+ 12V 电源 + FOC 节拍中断。
// 实例形态：文件内静态 handle，内含
//   - ops 指针、外设句柄（htim）
//   - 配置（三相通道映射、12V 使能引脚、PWM 周期计数）
// HAL_TIM_PeriodElapsedCallback 由本文件唯一持有（TIM8 属本文件资源），
// 上溢/下溢分别转发到 gate_register_isrs 注册的回调。
//
// TODO: 中断分发扩展性 —— 现在依赖"本文件唯一持有 PeriodElapsed 回调"的
//       约定；若将来有多路 TIM 需要同一回调，应改为集中分发。
// ============================================================
#include "gate_drivers.h"

#include "tim.h"

// ---------- 本板配置 ----------
#define GATE_TIC_PWM 2099U // 定时器周期计数值（ARR，对应 F_PWM=20kHz）

// 相 → 通道映射（与硬件相序一致）：A→CH3、B→CH2、C→CH1
#define GATE_PWM_CH_A TIM_CHANNEL_3
#define GATE_PWM_CH_B TIM_CHANNEL_2
#define GATE_PWM_CH_C TIM_CHANNEL_1

#define POWER12V_GPIO_PORT GPIOC
#define POWER12V_GPIO_PIN GPIO_PIN_13

// ---------- 实例 handle ----------
typedef struct
{
    TIM_HandleTypeDef *htim; // 外设：功率级定时器
    uint32_t ch_a;           // 配置：A 相通道
    uint32_t ch_b;           // 配置：B 相通道
    uint32_t ch_c;           // 配置：C 相通道
    GPIO_TypeDef *pwr_port;  // 配置：12V 使能 GPIO
    uint16_t pwr_pin;        // 配置：12V 使能引脚
    uint32_t tic_pwm;        // 配置：PWM 周期计数
} tGateHw;

static void gate_get_pwm_config(GateHandle h, uint32_t *pwm_period);
static void gate_power_ctrl(GateHandle h, bool on);
static void gate_start(GateHandle h);
static void gate_stop(GateHandle h);
static void gate_set_compare(GateHandle h, uint16_t ticA, uint16_t ticB, uint16_t ticC);

const tGateDrvOps gate_fd6288q_ops = {
    .get_pwm_config = gate_get_pwm_config,
    .power_ctrl = gate_power_ctrl,
    .start = gate_start,
    .stop = gate_stop,
    .set_compare = gate_set_compare,
};

// 静态实例
static tGateHw s_gate = {
    .htim = &htim8,
    .ch_a = GATE_PWM_CH_A,
    .ch_b = GATE_PWM_CH_B,
    .ch_c = GATE_PWM_CH_C,
    .pwr_port = POWER12V_GPIO_PORT,
    .pwr_pin = POWER12V_GPIO_PIN,
    .tic_pwm = GATE_TIC_PWM,
};

GateHandle gate_get_handle(void)
{
    return (GateHandle)&s_gate;
}

// ---- FOC 节拍中断（只在本文件定义） ----
static void (*s_sample_cb)(void) = NULL; // 上溢：电流采样点
static void (*s_ctrl_cb)(void) = NULL;   // 下溢：FOC 控制

void gate_register_isrs(void (*sample_cb)(void), void (*ctrl_cb)(void))
{
    s_sample_cb = sample_cb;
    s_ctrl_cb = ctrl_cb;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim != s_gate.htim)
        return;

    // 中心对齐：向下计数到 0 为下溢（采样点），向上计数到 ARR 为上溢（控制）
    if (htim->Instance->CR1 & TIM_CR1_DIR)
    {
        if (s_sample_cb)
            s_sample_cb();
    }
    else
    {
        if (s_ctrl_cb)
            s_ctrl_cb();
    }
}

// ---- ops 实现 ----

static void gate_get_pwm_config(GateHandle h, uint32_t *pwm_period)
{
    tGateHw *inst = (tGateHw *)h;
    if (!inst || !pwm_period)
        return;
    *pwm_period = inst->tic_pwm;
}

static void gate_power_ctrl(GateHandle h, bool on)
{
    tGateHw *inst = (tGateHw *)h;
    if (!inst)
        return;
    HAL_GPIO_WritePin(inst->pwr_port, inst->pwr_pin,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void gate_start(GateHandle h)
{
    tGateHw *inst = (tGateHw *)h;
    if (!inst || !inst->htim)
        return;

    HAL_TIM_PWM_Start(inst->htim, inst->ch_a);
    HAL_TIM_PWM_Start(inst->htim, inst->ch_b);
    HAL_TIM_PWM_Start(inst->htim, inst->ch_c);
    HAL_TIMEx_PWMN_Start(inst->htim, inst->ch_a);
    HAL_TIMEx_PWMN_Start(inst->htim, inst->ch_b);
    HAL_TIMEx_PWMN_Start(inst->htim, inst->ch_c);
}

static void gate_stop(GateHandle h)
{
    tGateHw *inst = (tGateHw *)h;
    if (!inst || !inst->htim)
        return;

    HAL_TIM_PWM_Stop(inst->htim, inst->ch_a);
    HAL_TIM_PWM_Stop(inst->htim, inst->ch_b);
    HAL_TIM_PWM_Stop(inst->htim, inst->ch_c);
    HAL_TIMEx_PWMN_Stop(inst->htim, inst->ch_a);
    HAL_TIMEx_PWMN_Stop(inst->htim, inst->ch_b);
    HAL_TIMEx_PWMN_Stop(inst->htim, inst->ch_c);
}

// 三相占空比：20kHz 热路径，直写 CCR 寄存器，绕开 __HAL_TIM_SetCompare 的通道判断
static void gate_set_compare(GateHandle h, uint16_t ticA, uint16_t ticB, uint16_t ticC)
{
    tGateHw *inst = (tGateHw *)h;
    if (!inst || !inst->htim)
        return;

    TIM_TypeDef *tim = inst->htim->Instance;
    tim->CCR3 = ticA; // 与上方 GATE_PWM_CH_A=CH3 对应
    tim->CCR2 = ticB;
    tim->CCR1 = ticC;
}
