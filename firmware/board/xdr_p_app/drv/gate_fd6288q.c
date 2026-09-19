// ============================================================
// gate_fd6288q.c — 电机功率级驱动（板级，直连 HAL）
//
// TIM8 中心对齐 6 路 PWM（CH1-3 + 互补）+ 12V 电源 + FOC 节拍中断。
// 实例形态：文件内静态 const 配置（外设 / 通道映射 / 使能引脚 / PWM 周期）。
// 中断：HAL_TIM_PeriodElapsedCallback 本体在 bsp_irq（全工程唯一持有处），
//       本驱动只注册处理函数，上溢/下溢转发到注册进来的 FOC 回调。
//
// 本文件实现 app/abs/gate_drv_board.h 的板级钩子。
// ============================================================
#include "gate_drv_board.h"

#include "bsp_irq.h"

#include "tim.h"

// ---------- 本板配置 ----------
#define GATE_TIC_PWM 2099U // 定时器周期计数值（ARR，对应 F_PWM=20kHz）

// 相 → 通道映射（与硬件相序一致）：A→CH3、B→CH2、C→CH1
#define GATE_PWM_CH_A TIM_CHANNEL_3
#define GATE_PWM_CH_B TIM_CHANNEL_2
#define GATE_PWM_CH_C TIM_CHANNEL_1

#define POWER12V_GPIO_PORT GPIOC
#define POWER12V_GPIO_PIN GPIO_PIN_13

// ---------- 实例配置（const，驻 Flash） ----------
typedef struct
{
    TIM_HandleTypeDef *htim; // 外设：功率级定时器
    uint32_t ch_a;           // A 相通道
    uint32_t ch_b;           // B 相通道
    uint32_t ch_c;           // C 相通道
    GPIO_TypeDef *pwr_port;  // 12V 使能 GPIO
    uint16_t pwr_pin;        // 12V 使能引脚
    uint32_t tic_pwm;        // PWM 周期计数
} tGateHw;

static const tGateHw s_gate = {
    .htim = &htim8,
    .ch_a = GATE_PWM_CH_A,
    .ch_b = GATE_PWM_CH_B,
    .ch_c = GATE_PWM_CH_C,
    .pwr_port = POWER12V_GPIO_PORT,
    .pwr_pin = POWER12V_GPIO_PIN,
    .tic_pwm = GATE_TIC_PWM,
};

// ---------- FOC 节拍中断 ----------
// HAL 回调本体在 bsp_irq；本驱动只注册处理函数。
static void (*s_sample_cb)(void) = NULL; // 下溢：电流采样点
static void (*s_ctrl_cb)(void) = NULL;   // 上溢：FOC 控制

static void gate_on_underflow(void *ctx)
{
    (void)ctx;
    if (s_sample_cb)
        s_sample_cb();
}

static void gate_on_overflow(void *ctx)
{
    (void)ctx;
    if (s_ctrl_cb)
        s_ctrl_cb();
}

void gate_board_register_isrs(void (*sample_cb)(void), void (*ctrl_cb)(void))
{
    s_sample_cb = sample_cb;
    s_ctrl_cb = ctrl_cb;

    // 首次调用时注册进集中分发（重复调用只更新上面的回调指针）
    static bool s_irq_bound = false;
    if (!s_irq_bound)
    {
        static const tTimIrq s_gate_irq = {
            .on_overflow = gate_on_overflow,
            .on_underflow = gate_on_underflow,
            .on_pulse_done = NULL,
            .ctx = NULL,
        };
        s_irq_bound = bsp_irq_bind_tim(s_gate.htim->Instance, &s_gate_irq);
    }
}

// ---------- 板级钩子实现 ----------

bool gate_board_open(uint32_t *pwm_period)
{
    if (!pwm_period || !s_gate.htim)
        return false;
    *pwm_period = s_gate.tic_pwm;
    return true;
}

void gate_board_power(bool on)
{
    HAL_GPIO_WritePin(s_gate.pwr_port, s_gate.pwr_pin,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void gate_board_enable(bool en)
{
    TIM_HandleTypeDef *htim = s_gate.htim;
    if (!htim)
        return;

    if (en)
    {
        HAL_TIM_PWM_Start(htim, s_gate.ch_a);
        HAL_TIM_PWM_Start(htim, s_gate.ch_b);
        HAL_TIM_PWM_Start(htim, s_gate.ch_c);
        HAL_TIMEx_PWMN_Start(htim, s_gate.ch_a);
        HAL_TIMEx_PWMN_Start(htim, s_gate.ch_b);
        HAL_TIMEx_PWMN_Start(htim, s_gate.ch_c);
    }
    else
    {
        HAL_TIM_PWM_Stop(htim, s_gate.ch_a);
        HAL_TIM_PWM_Stop(htim, s_gate.ch_b);
        HAL_TIM_PWM_Stop(htim, s_gate.ch_c);
        HAL_TIMEx_PWMN_Stop(htim, s_gate.ch_a);
        HAL_TIMEx_PWMN_Stop(htim, s_gate.ch_b);
        HAL_TIMEx_PWMN_Stop(htim, s_gate.ch_c);
    }
}

// 三相占空比：20kHz 热路径，直写 CCR 寄存器，绕开 __HAL_TIM_SetCompare 的通道判断
void gate_board_set_compare(uint16_t ticA, uint16_t ticB, uint16_t ticC)
{
    TIM_TypeDef *tim = s_gate.htim->Instance;
    tim->CCR3 = ticA; // 与上方 GATE_PWM_CH_A=CH3 对应
    tim->CCR2 = ticB;
    tim->CCR1 = ticC;
}
