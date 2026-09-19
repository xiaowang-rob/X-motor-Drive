#ifndef XDR_APP_ABS_ENCODER_BOARD_H
#define XDR_APP_ABS_ENCODER_BOARD_H

#include "encoder.h"

// ============================================================
// encoder_board.h — 编码器的板级钩子契约（abs 声明 / 板级实现）
//
// 这是"编译期绑定"的接缝：abs 的 encoder.c 只调用这三个函数，
// 具体芯片（MT6816 / MT6835 / AS5047）与 SPI/CS 细节全部由
// board/<B>/drv/board_encoder.c 一份实现承担，链接期解析。
//
// 板级实现语义：
//   open  按 type + chip 打开编码器（含 SPI 模式配置），输出单圈分辨率。
//         chip 为 ENC_CHIP_NONE 表示该路未装配 → 返回 false。
//   read  读一次原始角（0 ~ resolution-1）并打毫秒时间戳。
//         返回 false 表示本次读数不可信（通讯失败 / 磁场弱 / 校验错）。
//   abort 中止当前传输并复位总线（抬 CS），用于异常恢复。
//
// 板级实现须保证：read 是同步的、可重入性不要求（主循环 + 中断各一路时
// 由调用方串行化），且不阻塞超过一次 SPI 事务的时间。
// ============================================================

bool encoder_board_open(eEncoderType type, eEncoderChipId chip, uint16_t *resolution);
bool encoder_board_read(eEncoderType type, uint16_t *raw, uint32_t *ts_ms);
void encoder_board_abort(eEncoderType type);

#endif // XDR_APP_ABS_ENCODER_BOARD_H
