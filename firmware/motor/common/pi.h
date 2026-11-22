#ifndef MC_PI_H
#define MC_PI_H

/* 独立 PI：不包含微分、历史反馈或首次微分分支。
 * 初始化时完成 Ki 离散化，运行时只处理 P/I、限幅和抗积分饱和。
 * 调用前提：有效指针、有限参数、dt > 0、minimum <= maximum。 */
typedef struct {
    float kp, ki_dt;
    float integral, output;
    float minimum, maximum;
} tPI;

void pi_init(tPI *pi, float kp, float ki, float dt,
                float minimum, float maximum);
void pi_reset(tPI *pi);
float pi_update(tPI *pi, float reference, float feedback);
void pi_track(tPI *pi, float applied);

#endif
