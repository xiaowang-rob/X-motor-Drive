#ifndef XDR_APP_ABS_SENSE_BOARD_H
#define XDR_APP_ABS_SENSE_BOARD_H

#include "device.h"

// ============================================================
// sense_board.h — 采样的板级钩子契约（abs 声明 / 板级实现）
//
// 描述"由定时器触发、DMA 落缓冲的 ADC 采样前端"的最小能力。
// 实现 = board/<B>/drv/sen_mcu.c，直连 HAL，不持有业务逻辑。
//
// 板级实现语义：
//   open              启动采样前端（DMA + 首轮 Vbus/温度转换）并给出
//                     换算系数（cur_scale=A/码12bit、vbus_scale=V/码8bit）；
//                     返回前资源须可用，否则返回 false
//   set_sample_point  设置电流采样点在 PWM 周期内的位置（定时器比较值）
//   get_cur_raw       取最新一帧三相电流原始码（12bit，0~4095）
//   vt_trigger        触发一轮 Vbus/温度转换（软件触发）
//   get_vt_raw        取最新 Vbus/温度原始码；返回 false 表示无新帧
// ============================================================

bool sense_board_open(float *cur_scale, float *vbus_scale);
void sense_board_set_sample_point(uint32_t tic);
bool sense_board_get_cur_raw(uint16_t raw[3]);
void sense_board_vt_trigger(void);
bool sense_board_get_vt_raw(uint16_t *vbus_raw, uint16_t *temp_raw);

#endif // XDR_APP_ABS_SENSE_BOARD_H
