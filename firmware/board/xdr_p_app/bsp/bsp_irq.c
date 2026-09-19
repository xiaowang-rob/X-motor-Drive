// ============================================================
// bsp_irq.c — 中断集中分发（板级 bsp）
//
// 本文件是全工程**唯一**定义 HAL_*_Callback 的地方。
// 实现：固定大小绑定表（无堆、无动态分配），按外设 Instance 线性查找。
//       外设种类少（TIM/ADC/UART/CAN 各十余路），线性查找在中断里
//       只需几次指针比较，开销可忽略，换取零分配与零初始化顺序依赖。
// ============================================================

#include "bsp_irq.h"

// ---- 绑定表容量（按本板外设数量给足余量） ----
#define BSP_IRQ_TIM_MAX 6U
#define BSP_IRQ_ADC_MAX 2U
#define BSP_IRQ_UART_MAX 4U
#define BSP_IRQ_CAN_MAX 2U

typedef struct
{
    TIM_TypeDef *periph;
    tTimIrq irq;
    bool used;
} tTimSlot;

typedef struct
{
    ADC_TypeDef *periph;
    tAdcIrq irq;
    bool used;
} tAdcSlot;

typedef struct
{
    USART_TypeDef *periph;
    tUartIrq irq;
    bool used;
} tUartSlot;

typedef struct
{
    CAN_TypeDef *periph;
    tCanIrq irq;
    bool used;
} tCanSlot;

static tTimSlot s_tim[BSP_IRQ_TIM_MAX];
static tAdcSlot s_adc[BSP_IRQ_ADC_MAX];
static tUartSlot s_uart[BSP_IRQ_UART_MAX];
static tCanSlot s_can[BSP_IRQ_CAN_MAX];

// ---- 绑定 ----

#define BSP_IRQ_BIND_TEMPLATE(SLOTS, N, TYPEF, MEMBER, ARG)                        \
    do                                                                             \
    {                                                                              \
        if (!(ARG) || !irq)                                                        \
            return false;                                                          \
        for (uint32_t i = 0U; i < (N); i++)                                        \
        {                                                                          \
            if ((SLOTS)[i].used)                                                   \
            {                                                                      \
                if ((SLOTS)[i].MEMBER == (ARG))                                    \
                    return false; /* 该外设已绑定 */                               \
                continue;                                                          \
            }                                                                      \
            (SLOTS)[i].MEMBER = (ARG);                                             \
            (SLOTS)[i].irq = *irq;                                                 \
            (SLOTS)[i].used = true;                                                \
            return true;                                                           \
        }                                                                          \
        return false; /* 表满 */                                                   \
    } while (0)

bool bsp_irq_bind_tim(TIM_TypeDef *tim, const tTimIrq *irq)
{
    BSP_IRQ_BIND_TEMPLATE(s_tim, BSP_IRQ_TIM_MAX, tTimSlot, periph, tim);
}

bool bsp_irq_bind_adc(ADC_TypeDef *adc, const tAdcIrq *irq)
{
    BSP_IRQ_BIND_TEMPLATE(s_adc, BSP_IRQ_ADC_MAX, tAdcSlot, periph, adc);
}

bool bsp_irq_bind_uart(USART_TypeDef *uart, const tUartIrq *irq)
{
    BSP_IRQ_BIND_TEMPLATE(s_uart, BSP_IRQ_UART_MAX, tUartSlot, periph, uart);
}

bool bsp_irq_bind_can(CAN_TypeDef *can, const tCanIrq *irq)
{
    BSP_IRQ_BIND_TEMPLATE(s_can, BSP_IRQ_CAN_MAX, tCanSlot, periph, can);
}

// ---- 查找 ----

static tTimSlot *find_tim(TIM_TypeDef *tim)
{
    for (uint32_t i = 0U; i < BSP_IRQ_TIM_MAX; i++)
        if (s_tim[i].used && s_tim[i].periph == tim)
            return &s_tim[i];
    return NULL;
}

static tAdcSlot *find_adc(ADC_TypeDef *adc)
{
    for (uint32_t i = 0U; i < BSP_IRQ_ADC_MAX; i++)
        if (s_adc[i].used && s_adc[i].periph == adc)
            return &s_adc[i];
    return NULL;
}

static tUartSlot *find_uart(USART_TypeDef *uart)
{
    for (uint32_t i = 0U; i < BSP_IRQ_UART_MAX; i++)
        if (s_uart[i].used && s_uart[i].periph == uart)
            return &s_uart[i];
    return NULL;
}

static tCanSlot *find_can(CAN_TypeDef *can)
{
    for (uint32_t i = 0U; i < BSP_IRQ_CAN_MAX; i++)
        if (s_can[i].used && s_can[i].periph == can)
            return &s_can[i];
    return NULL;
}

// ---- 唯一的 HAL 回调定义处 ----

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    tTimSlot *s = find_tim(htim->Instance);
    if (!s)
        return;

    // 中心对齐：CR1.DIR=1 表示当前向下计数（下溢事件），否则向上（上溢）
    if (htim->Instance->CR1 & TIM_CR1_DIR)
    {
        if (s->irq.on_underflow)
            s->irq.on_underflow(s->irq.ctx);
    }
    else
    {
        if (s->irq.on_overflow)
            s->irq.on_overflow(s->irq.ctx);
    }
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    tTimSlot *s = find_tim(htim->Instance);
    if (s && s->irq.on_pulse_done)
        s->irq.on_pulse_done(s->irq.ctx);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    tAdcSlot *s = find_adc(hadc->Instance);
    if (s && s->irq.on_conv_cplt)
        s->irq.on_conv_cplt(s->irq.ctx);
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
    (void)hadc; // 采样错误：暂只记录，由上层状态字体现
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    tUartSlot *s = find_uart(huart->Instance);
    if (!s || !s->irq.on_rx_event)
        return;

    HAL_UART_RxEventTypeTypeDef ev = HAL_UARTEx_GetRxEventType(huart);
    // 半传输(HT) 不上报 —— 否则会把半个缓冲当成一帧
    if (ev == HAL_UART_RXEVENT_IDLE)
        s->irq.on_rx_event(s->irq.ctx, Size, true);
    else if (ev == HAL_UART_RXEVENT_TC)
        s->irq.on_rx_event(s->irq.ctx, Size, false);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    tUartSlot *s = find_uart(huart->Instance);
    if (s && s->irq.on_error)
        s->irq.on_error(s->irq.ctx);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    tCanSlot *s = find_can(hcan->Instance);
    if (s && s->irq.on_rx_pending)
        s->irq.on_rx_pending(s->irq.ctx);
}
