#ifndef __FILTER_H
#define __FILTER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// 中位值滤波和防脉冲滤波的最大窗口大小（可根据需要调整）
#ifndef MAX_MEDIAN_FILTER_SIZE
#define MAX_MEDIAN_FILTER_SIZE  64
#endif

#ifndef MAX_PULSE_FILTER_SIZE
#define MAX_PULSE_FILTER_SIZE   64
#endif

// 限幅滤波
typedef struct {
    int max_deviation;
    int last_value;
} tAmplitudeLimitingFilter;

void filter_amplitude_limiting_init(tAmplitudeLimitingFilter *al,
                                    int max_deviation, int initial_value);
int  filter_amplitude_limiting(tAmplitudeLimitingFilter *al, int new_value);

// 中位值滤波
typedef struct {
    int *buffer;
    int  size;
    int  index;
} tMedianFilter;

void filter_median_init(tMedianFilter *filter, int *buffer, int size);
int  filter_median(tMedianFilter *filter, int new_value);

// 滑动平均滤波（浮点）
typedef struct {
    float *buffer;
    int    size;
    int    index;
    float  sum;
    bool   is_full;
} tMovingAverageFilter;

void   filter_moving_avg_init(tMovingAverageFilter *filter,
                              float *buffer, int size);
float  filter_moving_average(tMovingAverageFilter *filter, float new_value);

// 加权滑动平均滤波（整型）
typedef struct {
    int *buffer;
    int *coeff;
    int  size;
    int  index;
    int  coeff_sum;
} tWeightedMovingAverageFilter;

void filter_weighted_moving_avg_init(tWeightedMovingAverageFilter *filter,
                                     int *buffer, int *coeff, int size);
int  filter_weighted_moving_avg(tWeightedMovingAverageFilter *filter, int new_value);

// 一阶滞后滤波（低通）
typedef struct {
    float alpha;
    float last_value;
} tFirstOrderLagFilter;

void  filter_first_order_lag_init(tFirstOrderLagFilter *filter,
                                  float alpha, float initial_value);
float filter_first_order_lag(tFirstOrderLagFilter *filter, float new_value);

// 卡尔曼滤波（一维标量）
typedef struct {
    float q, r, x, p, k;
} tKalmanFilter;

void  filter_kalman_init(tKalmanFilter *filter,
                         float q, float r, float initial_value);
float filter_kalman(tKalmanFilter *filter, float measurement);

// 防脉冲干扰平均滤波（整型）
typedef struct {
    uint16_t *buffer;
    uint8_t   size;
    uint8_t   index;
} tPulseInterferenceFilter;

void     filter_pulse_init(tPulseInterferenceFilter *filter,
                           uint16_t *buffer, uint8_t size);
uint16_t filter_pulse(tPulseInterferenceFilter *filter, uint16_t new_value);

// 算术平均滤波（一次性计算，无状态）
int filter_arithmetic_mean(const int *data_buf, int size);

// 巴特沃斯低通滤波（需CMSIS-DSP）
#ifdef ARM_MATH_H
typedef struct {
    arm_biquad_casd_df1_inst_f32 inst;
    float32_t state[4];
    float32_t coeffs[5];
} tBW_FilterInstance;

void    filter_butterworth_init(tBW_FilterInstance *f, float32_t *coeffs);
float32_t filter_butterworth_process(tBW_FilterInstance *f, float32_t input);
void    filter_butterworth_reset(tBW_FilterInstance *f);
#endif

#endif /* __FILTER_H */