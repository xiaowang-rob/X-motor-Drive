// ============================================================
// gate_fd6288q.c — 电机功率级驱动（板级，直连 HAL）
//
// TIM1 中心对齐 6 路 PWM（CH1-3 + 互补）+ 12V 电源。
// 本板外设：TIM1（Core/Src/tim.c 的 htim1，PA8/9/10=CH1/2/3，
//           PB13/14/15=CH1N/2N/3N，ARR=4249 → 20kHz）。
// 实例形态：外部链接实例（外设 / 通道映射 / 使能引脚 / PWM 周期）。
// 中断：FOC 节拍回调（上/下溢）由板级 bsp_irq.c 独占定义，本驱动通过
//       fd6288q_owns_tim() 供其判定事件源，不定义 HAL 回调符号。
//
// 实现 abs/gate_drv.h 的 tGateOps。
// ============================================================
#include "gate_fd6288q.h"

#include "tim.h"

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
};

tFd6288q g_fd6288q = {
    .htim = &htim1,
    .ch_a = GATE_PWM_CH_A,
    .ch_b = GATE_PWM_CH_B,
    .ch_c = GATE_PWM_CH_C,
    .pwr_port = POWER12V_GPIO_PORT,
    .pwr_pin = POWER12V_GPIO_PIN,
    .tic_pwm = GATE_TIC_PWM,
};

// ---------- 事件源判定（供 bsp_irq.c 的 HAL 回调过滤） ----------
bool fd6288q_owns_tim(const TIM_HandleTypeDef *htim)
{
    return (htim != NULL) && (g_fd6288q.htim != NULL) &&
           (htim->Instance == g_fd6288q.htim->Instance);
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

// ---- 驱动出口 ----
const tGateOps fd6288q_ops = {
    .open = fd6288q_open,
    .power = fd6288q_power,
    .enable = fd6288q_enable,
    .set_compare = fd6288q_set_compare,
};
