// ============================================================
// bsp_mcu_pwm.c — 电机功率级驱动（板级，直连 LL）
//
// TIM1 中心对齐 6 路 PWM（CH1-3 + 互补）+ 12V 电源。
// 本板外设：TIM1（Core/Src/tim.c 的 htim1，PA8/9/10=CH1/2/3，
//           PB13/14/15=CH1N/2N/3N，ARR=4249 → 20kHz）。
// 实例形态：外部链接实例（外设 / 通道映射 / 使能引脚 / PWM 周期）。
// TIM1 更新中断从 Core 的 USER CODE 区转发到 bsp_pwm_irq。
//
// 实现 abs/pwm.h 的 tGateOps。
// ============================================================
#include "bsp_adc.h"
#include "bsp_mcu.h"
#include "bsp_pwm.h"

// ---------- 实例 ----------
typedef struct tMcuPwm {
    TIM_TypeDef *htim;      // 外设：功率级定时器
    GPIO_TypeDef *pwr_port; // 12V 使能 GPIO
    uint16_t pwr_pin;       // 12V 使能引脚

} tMcuPwm;

tMcuPwm mcu_pwm = {
    .htim = PWM_TIM,
    .pwr_port = POWER12V_GPIO_PORT,
    .pwr_pin = POWER12V_GPIO_PIN,
};
// PWM 节拍回调（由 pwm_register_callback 注入）
static void (*pwm_up_callback)(void) = NULL;
static void (*pwm_down_callback)(void) = NULL;

// ---------- 功率级节拍中断 ----------

void bsp_pwm_irq(void) {
    if (!LL_TIM_IsActiveFlag_UPDATE(mcu_pwm.htim) ||
        !LL_TIM_IsEnabledIT_UPDATE(mcu_pwm.htim))
        return;
    LL_TIM_ClearFlag_UPDATE(mcu_pwm.htim);
    // 中心对齐：更新事件后 DIR=0 为下溢，DIR=1 为上溢。
    if (LL_TIM_GetDirection(mcu_pwm.htim) == LL_TIM_COUNTERDIRECTION_UP) {
        if (pwm_down_callback)
            pwm_down_callback();
    } else if (pwm_up_callback) {
        pwm_up_callback();
    }
}

void pwm_register_callback(void (*up_cb)(void), void (*down_cb)(void)) {
    pwm_up_callback = up_cb;
    pwm_down_callback = down_cb;
    LL_TIM_ClearFlag_UPDATE(mcu_pwm.htim);
    LL_TIM_EnableIT_UPDATE(mcu_pwm.htim);
}

// ---------- 驱动接口（tGateOps） ----------

static void mcu_pwm_open(void *dev, tPwmCfg *cfg) {
    tMcuPwm *pwm = (tMcuPwm *)dev;
    if (!pwm)
        return;
    cfg->pwm_arr = PWM_ARR, cfg->ticTd = GATE_T_DIED * PWM_FREQ * PWM_ARR;
    cfg->ticTn = GATE_T_NOISE * PWM_FREQ * PWM_ARR;
    cfg->ticTs = ADC_T_SAMPLE_CUR * PWM_FREQ * PWM_ARR;
}

static void gate_drv_power(void *dev, bool on) {
    tMcuPwm *pwm = (tMcuPwm *)dev;
    if (!pwm)
        return;
    HAL_GPIO_WritePin(
        pwm->pwr_port, pwm->pwr_pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void mcu_pwm_enable(void *dev, bool en) {
    tMcuPwm *pwm = (tMcuPwm *)dev;
    if (!pwm || !pwm->htim)
        return;

    uint32_t channels = LL_TIM_CHANNEL_CH1 | LL_TIM_CHANNEL_CH2 |
                        LL_TIM_CHANNEL_CH3 | LL_TIM_CHANNEL_CH1N |
                        LL_TIM_CHANNEL_CH2N | LL_TIM_CHANNEL_CH3N;
    if (en) {
        LL_TIM_CC_EnableChannel(pwm->htim, channels);
        LL_TIM_EnableAllOutputs(pwm->htim);
        LL_TIM_EnableCounter(pwm->htim);
    } else {
        LL_TIM_CC_DisableChannel(pwm->htim, channels);
    }
}

// 三相占空比：20kHz 热路径，直写 CCR 寄存器，绕开 __HAL_TIM_SetCompare
// 的通道判断
static void mcu_pwm_set_compare(void *dev, uint16_t ticU, uint16_t ticV,
                                uint16_t ticW) {
    const tMcuPwm *pwm = (const tMcuPwm *)dev;

    pwm->htim->GATE_PWM_CH_U = ticU; // 与上方 GATE_PWM_CH_U=CH3 对应
    pwm->htim->GATE_PWM_CH_V = ticV;
    pwm->htim->GATE_PWM_CH_W = ticW;
}

// ---- 驱动出口 ----
const tPwmOps mcu_pwm_ops = {
    .open = mcu_pwm_open,
    .power = gate_drv_power,
    .enable = mcu_pwm_enable,
    .set_compare = mcu_pwm_set_compare,
};

static const tPwmDevHandle s_dev_handle = {
    .ops = &mcu_pwm_ops,
    .dev = &mcu_pwm,
};

const tPwmDevHandle *Get_MCU_PWMHandle(void) { return &s_dev_handle; }
