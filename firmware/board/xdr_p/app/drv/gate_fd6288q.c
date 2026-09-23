// ============================================================
// gate_fd6288q.c — 电机功率级驱动（板级，直连 HAL）
//
// TIM8 中心对齐 6 路 PWM（CH1-3 + 互补）+ 12V 电源 + FOC 节拍中断。
// 实例形态：外部链接实例（外设 / 通道映射 / 使能引脚 / PWM 周期 / FOC 回调）。
// 中断：HAL_TIM_PeriodElapsedCallback 本体在 bsp_irq（全工程唯一持有处），
//       本驱动只注册处理函数，上溢/下溢转发到注册进来的 FOC 回调。
//
// 实现 abs/gate_drv.h 的 tGateOps。
// ============================================================
#include "gate_fd6288q.h"

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

// ---------- 实例 ----------
struct tFd6288q
{
    TIM_HandleTypeDef *htim; // 外设：功率级定时器
    uint32_t ch_a;           // A 相通道
    uint32_t ch_b;           // B 相通道
    uint32_t ch_c;           // C 相通道
    GPIO_TypeDef *pwr_port;  // 12V 使能 GPIO
    uint16_t pwr_pin;        // 12V 使能引脚
    uint32_t tic_pwm;        // PWM 周期计数

    void (*sample_cb)(void); // 下溢：电流采样点（由 gate_drv 注册）
    void (*ctrl_cb)(void);   // 上溢：FOC 控制（由 gate_drv 注册）
};

tFd6288q g_fd6288q = {
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
static void gate_on_underflow(void *ctx)
{
    tFd6288q *inst = (tFd6288q *)ctx;
    if (inst && inst->sample_cb)
        inst->sample_cb();
}

static void gate_on_overflow(void *ctx)
{
    tFd6288q *inst = (tFd6288q *)ctx;
    if (inst && inst->ctrl_cb)
        inst->ctrl_cb();
}

// ---------- 驱动接口（tGateOps） ----------

static bool fd6288q_open(void *handle, uint32_t *pwm_period)
{
    tFd6288q *inst = (tFd6288q *)handle;
    if (!inst || !inst->htim || !pwm_period)
        return false;
    *pwm_period = inst->tic_pwm;
    return true;
}

static void fd6288q_power(void *handle, bool on)
{
    tFd6288q *inst = (tFd6288q *)handle;
    if (!inst)
        return;
    HAL_GPIO_WritePin(inst->pwr_port, inst->pwr_pin,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void fd6288q_enable(void *handle, bool en)
{
    tFd6288q *inst = (tFd6288q *)handle;
    if (!inst || !inst->htim)
        return;

    TIM_HandleTypeDef *htim = inst->htim;
    if (en)
    {
        HAL_TIM_PWM_Start(htim, inst->ch_a);
        HAL_TIM_PWM_Start(htim, inst->ch_b);
        HAL_TIM_PWM_Start(htim, inst->ch_c);
        HAL_TIMEx_PWMN_Start(htim, inst->ch_a);
        HAL_TIMEx_PWMN_Start(htim, inst->ch_b);
        HAL_TIMEx_PWMN_Start(htim, inst->ch_c);
    }
    else
    {
        HAL_TIM_PWM_Stop(htim, inst->ch_a);
        HAL_TIM_PWM_Stop(htim, inst->ch_b);
        HAL_TIM_PWM_Stop(htim, inst->ch_c);
        HAL_TIMEx_PWMN_Stop(htim, inst->ch_a);
        HAL_TIMEx_PWMN_Stop(htim, inst->ch_b);
        HAL_TIMEx_PWMN_Stop(htim, inst->ch_c);
    }
}

// 三相占空比：20kHz 热路径，直写 CCR 寄存器，绕开 __HAL_TIM_SetCompare 的通道判断
static void fd6288q_set_compare(void *handle, uint16_t ticA, uint16_t ticB, uint16_t ticC)
{
    const tFd6288q *inst = (const tFd6288q *)handle;
    if (!inst || !inst->htim)
        return;
    TIM_TypeDef *tim = inst->htim->Instance;
    tim->CCR3 = ticA; // 与上方 GATE_PWM_CH_A=CH3 对应
    tim->CCR2 = ticB;
    tim->CCR1 = ticC;
}

static void fd6288q_set_isr(void *handle, void (*sample_cb)(void), void (*ctrl_cb)(void))
{
    tFd6288q *inst = (tFd6288q *)handle;
    if (!inst || !inst->htim)
        return;

    inst->sample_cb = sample_cb;
    inst->ctrl_cb = ctrl_cb;

    // 首次调用时注册进集中分发（重复调用只更新上面的回调指针）
    static bool s_irq_bound = false;
    if (!s_irq_bound)
    {
        static const tTimIrq s_gate_irq = {
            .on_overflow = gate_on_overflow,
            .on_underflow = gate_on_underflow,
            .on_pulse_done = NULL,
            .ctx = &g_fd6288q,
        };
        s_irq_bound = bsp_irq_bind_tim(inst->htim->Instance, &s_gate_irq);
    }
}

// ---- 驱动出口 ----
const tGateOps fd6288q_ops = {
    .open = fd6288q_open,
    .power = fd6288q_power,
    .enable = fd6288q_enable,
    .set_compare = fd6288q_set_compare,
    .set_isr = fd6288q_set_isr,
};
