// ============================================================
// gate_fd6288q.c — 电机功率级驱动（板级，直连 HAL）
//
// TIM1 中心对齐 6 路 PWM（CH1-3 + 互补）+ 12V 电源 + FOC 节拍中断。
// 本板外设：TIM1（Core/Src/tim.c 的 htim1，PA8/9/10=CH1/2/3，
//           PB13/14/15=CH1N/2N/3N，ARR=4249 → 20kHz）。
// 实例形态：外部链接实例（外设 / 通道映射 / 使能引脚 / PWM 周期 / FOC 回调）。
// 中断：HAL_TIM_PeriodElapsedCallback 由本驱动定义（本板功率级只有 TIM1
//       一路），上溢/下溢分别转发到注册进来的 FOC 回调。
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

    void (*sample_cb)(void); // 下溢：电流采样点（由 gate_drv 注册）
    void (*ctrl_cb)(void);   // 上溢：FOC 控制（由 gate_drv 注册）
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

// ---------- FOC 节拍中断 ----------
// HAL 回调由本驱动独占定义；采样/控制回调在中断上下文运行，须短小。
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (!g_fd6288q.htim || htim->Instance != g_fd6288q.htim->Instance)
        return;

    // 中心对齐：CR1.DIR=1 表示当前向下计数（下溢事件），否则向上（上溢）
    if (htim->Instance->CR1 & TIM_CR1_DIR)
    {
        if (g_fd6288q.sample_cb)
            g_fd6288q.sample_cb();
    }
    else
    {
        if (g_fd6288q.ctrl_cb)
            g_fd6288q.ctrl_cb();
    }
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
}

// ---- 驱动出口 ----
const tGateOps fd6288q_ops = {
    .open = fd6288q_open,
    .power = fd6288q_power,
    .enable = fd6288q_enable,
    .set_compare = fd6288q_set_compare,
    .set_isr = fd6288q_set_isr,
};
