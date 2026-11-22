# xmath

数学运算统一通过 `xmath.h`。新调用采用 `xmath_<operation>_f32/f64`，
其中 f32 接收/返回 float，f64 接收/返回 double。角度统一用弧度；
三角函数、开方在 DSP/CORDIC 后端使用对应实现，双精度轨迹始终保留 double。

| 运算 | 接口 |
| --- | --- |
| 有限值判断 | `xmath_isfinite_f32/f64` |
| 绝对值、最小、最大、限幅 | `xmath_abs/min/max/clamp_f32/f64` |
| 符号 | `xmath_sign_f32`、`xmath_copysign_f32/f64` |
| 非负浮点到无符号整数舍入 | `xmath_round_u32_f32`，输入和舍入结果须在 uint32_t 范围内 |
| 开方 | `xmath_sqrt_f32/f64`、`xmath_cbrt_f64` |
| 指数、双曲正切 | `xmath_exp_f32`、`xmath_tanh_f32` |
| 三角函数 | `xmath_sin/cos/sincos/atan2_f32` |
| 有界二维模平方 | `xmath_norm_squared_f32`，限幅先比较平方，仅超限时开方 |
| 二维模长 | `xmath_hypot_f32`，缩放后计算，避免中间平方溢出 |
| 角度归一化 | `xmath_wrap_two_pi_f32`、`xmath_wrap_pi_f32` |

共享数学常量使用 `XMATH_*_F32`。旧的 `XSIN`、`XCLAMP`、`XMATH_PI` 等
仅保留兼容转发/别名，其他模块可以逐步迁移，不需要同时改动应用与协议。

调用者保证有限输入、有效指针和数学定义域（例如开方输入非负）。
基础运算不额外检查 NaN/Inf；`min/max` 使用直接比较，不承诺 NaN 语义。
角度归一化输出范围是 [0, 2pi) / [-pi, pi)，大角度使用浮点余数。
需要判断有限值的上层仍可显式调用 `xmath_isfinite_f32/f64`。

CMake `XMATH_BACKEND` 可选 `DSP`、`LIBM`、`CORDIC`：

- 固件默认为 DSP，使用板卡提供的 CMSIS-DSP。
- 独立主机 math/motor 构建默认为 LIBM，不需要 CMSIS/HAL。
- CORDIC 为 STM32G4 实现，集成方还需提供 STM32G4 LL/设备头文件及目标芯片宏。
  外设为共享资源，调用方需保证不同上下文不会交叉使用 CORDIC 寄存器。

后端宏由目标传播，分别为 `MATH_LIB_DSP`、`MATH_LIB_LIBM`、`MATH_LIB_CORDIC`；
只能选择一个。非 CMake 调用未指定宏时继续沿用 DSP 默认值。

```sh
cmake -S firmware/math -B /tmp/xmath-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/xmath-tests
ctest --test-dir /tmp/xmath-tests --output-on-failure
```

数值回归覆盖精度、角度边界、大/小数模长及旧接口兼容性。
DSP/CORDIC 的目标平台编译检查不替代硬件数值与时序验证。
