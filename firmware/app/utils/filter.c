#include "filter.h"
#include <string.h>
#include <assert.h>


// 内部冒泡排序
static void bubble_sort(int arr[], int n)
{
    if (!arr || n <= 1) return;
    for (int i = 0; i < n - 1; i++) {
        int swapped = 0;
        for (int j = 0; j < n - 1 - i; j++) {
            if (arr[j] > arr[j + 1]) {
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
                swapped = 1;
            }
        }
        if (!swapped) break;
    }
}

// 限幅滤波
void filter_amplitude_limiting_init(tAmplitudeLimitingFilter *al,
                                    int max_deviation, int initial_value)
{
    assert(al != NULL);
    al->max_deviation = max_deviation;
    al->last_value = initial_value;
}

int filter_amplitude_limiting(tAmplitudeLimitingFilter *al, int new_value)
{
    assert(al != NULL);
    // 使用 long long 避免减法溢出
    long long diff = (long long)new_value - (long long)al->last_value;
    if (diff < 0) diff = -diff;
    if (diff > al->max_deviation) return al->last_value;
    al->last_value = new_value;
    return new_value;
}

// 中位值滤波
void filter_median_init(tMedianFilter *filter, int *buffer, int size)
{
    assert(filter != NULL && buffer != NULL && size > 0);
    filter->buffer = buffer;
    filter->size = size;
    filter->index = 0;
    memset(buffer, 0, size * sizeof(int));
}

int filter_median(tMedianFilter *filter, int new_value)
{
    if (NULL == filter || NULL == filter->buffer || filter->size <= 0)
        return new_value;
    if (filter->size > MAX_MEDIAN_FILTER_SIZE)
        return new_value;   // 窗口过大，安全返回原值

    int buf[MAX_MEDIAN_FILTER_SIZE];   // 固定大小，避免 VLA 栈溢出
    int size = filter->size;

    filter->buffer[filter->index] = new_value;
    filter->index = (filter->index + 1) % size;

    memcpy(buf, filter->buffer, size * sizeof(int));
    bubble_sort(buf, size);
    return buf[(size - 1) / 2];
}

// 滑动平均滤波（浮点）
void filter_moving_avg_init(tMovingAverageFilter *filter,
                            float *buffer, int size)
{
    assert(filter != NULL && buffer != NULL && size > 0);
    filter->buffer = buffer;
    filter->size = size;
    filter->index = 0;
    filter->sum = 0.0f;
    filter->is_full = false;
    memset(buffer, 0, size * sizeof(float));
}

float filter_moving_average(tMovingAverageFilter *filter, float new_value)
{
    assert(filter != NULL);
    int size = filter->size;

    if (filter->is_full) {
        filter->sum -= filter->buffer[filter->index];
    }
    filter->buffer[filter->index] = new_value;
    filter->sum += new_value;

    filter->index = (filter->index + 1) % size;
    if (filter->index == 0) filter->is_full = true;

    int count = filter->is_full ? size : filter->index;
    // 第一次调用时 count 为 0，直接返回新值
    if (count == 0) return new_value;
    return filter->sum / count;
}

// 加权滑动平均滤波（整型）
void filter_weighted_moving_avg_init(tWeightedMovingAverageFilter *filter,
                                     int *buffer, int *coeff, int size)
{
    assert(filter != NULL && buffer != NULL && coeff != NULL && size > 0);
    filter->buffer = buffer;
    filter->coeff = coeff;
    filter->size = size;
    filter->index = 0;
    filter->coeff_sum = 0;
    for (int i = 0; i < size; i++) {
        filter->coeff_sum += coeff[i];
    }
    // 若系数和为零，置为 1 避免除零
    if (filter->coeff_sum == 0) {
        filter->coeff_sum = 1;
    }
    memset(buffer, 0, size * sizeof(int));
}

int filter_weighted_moving_avg(tWeightedMovingAverageFilter *filter, int new_value)
{
    assert(filter != NULL);
    int size = filter->size;
    filter->buffer[filter->index] = new_value;
    filter->index = (filter->index + 1) % size;

    long long weighted_sum = 0;
    for (int i = 0; i < size; i++) {
        int buf_index = (filter->index + i) % size;
        weighted_sum += (long long)filter->buffer[buf_index] * filter->coeff[i];
    }
    return (int)(weighted_sum / filter->coeff_sum);
}

// 一阶滞后滤波
void filter_first_order_lag_init(tFirstOrderLagFilter *filter,
                                 float alpha, float initial_value)
{
    assert(filter != NULL);
    filter->alpha = alpha;
    filter->last_value = initial_value;
}

float filter_first_order_lag(tFirstOrderLagFilter *filter, float new_value)
{
    assert(filter != NULL);
    filter->last_value = filter->alpha * new_value +
                         (1.0f - filter->alpha) * filter->last_value;
    return filter->last_value;
}

// 卡尔曼滤波
void filter_kalman_init(tKalmanFilter *filter,
                        float q, float r, float initial_value)
{
    assert(filter != NULL);
    filter->q = q;
    filter->r = r;
    filter->x = initial_value;
    filter->p = 1.0f;
    filter->k = 0.0f;
}

float filter_kalman(tKalmanFilter *filter, float measurement)
{
    assert(filter != NULL);
    filter->p = filter->p + filter->q;
    filter->k = filter->p / (filter->p + filter->r);
    filter->x = filter->x + filter->k * (measurement - filter->x);
    filter->p = (1.0f - filter->k) * filter->p;
    return filter->x;
}

// 防脉冲干扰平均滤波
void filter_pulse_init(tPulseInterferenceFilter *filter,
                       uint16_t *buffer, uint8_t size)
{
    assert(filter != NULL && buffer != NULL && size >= 3);
    filter->buffer = buffer;
    filter->size = size;
    filter->index = 0;
    memset(buffer, 0, size * sizeof(uint16_t));
}

uint16_t filter_pulse(tPulseInterferenceFilter *filter, uint16_t new_value)
{
    if (NULL == filter || NULL == filter->buffer || filter->size < 3)
        return new_value;   // 窗口过小无法防脉冲，直接返回原值
    if (filter->size > MAX_PULSE_FILTER_SIZE)
        return new_value;   // 窗口过大，安全返回原值

    int size = filter->size;
    filter->buffer[filter->index] = new_value;
    filter->index = (filter->index + 1) % size;

    int temp[MAX_PULSE_FILTER_SIZE];   // 固定大小
    for (int i = 0; i < size; i++) {
        temp[i] = (int)filter->buffer[i];
    }
    bubble_sort(temp, size);

    uint32_t sum = 0;
    for (int i = 1; i < size - 1; i++) {
        sum += (uint32_t)temp[i];
    }
    return (uint16_t)(sum / (size - 2));
}

// 算术平均滤波
int filter_arithmetic_mean(const int *data_buf, int size)
{
    if (NULL == data_buf || size <= 0) return 0;   // 避免除零
    long long sum = 0;
    for (int i = 0; i < size; i++) {
        sum += data_buf[i];
    }
    return (int)(sum / size);
}

// 巴特沃斯滤波（条件编译）
#ifdef ARM_MATH_H
#include "arm_math.h"   // 确保 CMSIS-DSP 头文件已包含

void filter_butterworth_init(tBW_FilterInstance *f, float32_t *coeffs)
{
    assert(f != NULL && coeffs != NULL);
    memset(f, 0, sizeof(tBW_FilterInstance));
    for (int i = 0; i < 5; i++) {
        f->coeffs[i] = coeffs[i];
    }
    arm_biquad_cascade_df1_init_f32(&f->inst, 1, f->coeffs, f->state);
}

float32_t filter_butterworth_process(tBW_FilterInstance *f, float32_t input)
{
    assert(f != NULL);
    float32_t output;
    arm_biquad_cascade_df1_f32(&f->inst, &input, &output, 1);
    return output;
}

void filter_butterworth_reset(tBW_FilterInstance *f)
{
    assert(f != NULL);
    memset(f->state, 0, sizeof(f->state));
}
#endif