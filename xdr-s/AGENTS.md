# Repository Guidelines

## Project Structure

- `Core/Src/` and `Core/Inc/` contain the STM32G474 firmware entry point, interrupt handlers, and peripheral setup. Open `XDr-S.ioc` in STM32CubeMX to change pin or peripheral configuration. Keep manual edits to generated code inside the `USER CODE` blocks so they survive regeneration.
- `MOTOR_DRIVE/motor/` holds motor control, commands, configuration, and algorithms. Shared CRC and filtering code lives in `MOTOR_DRIVE/utils/`, and board math support lives in `BSP/math/`.
- `Drivers/` and `Middlewares/` contain vendor code. The startup assembly and flash linker script live at the root. Firmware build rules live in `CMakeLists.txt` and `cmake/`.

## Build and Run

Use CMake 3.22 or newer, Ninja, and the Arm GNU toolchain (`arm-none-eabi-gcc` and `arm-none-eabi-g++` on `PATH`). Run from the repository root.

- `cmake --preset Debug && cmake --build --preset Debug` builds `build/Debug/XDr-S.elf` with debug symbols.
- `cmake --preset Release && cmake --build --preset Release` builds the size-optimized firmware under `build/Release/`.
- Load the ELF onto the STM32G474 target with a compatible debug probe to run it on hardware. The firmware presets currently build the CubeMX application and drivers. They leave the motor modules out.

## Coding Style

Use C11 and match the file’s indentation. Motor code uses four spaces and opening braces on the same line. Generated Core files typically use two-space indentation. Follow names such as `mc_foc.c`, `motor_init`, and `MC_FAULT_EXTERNAL`. The GCC toolchain enables `-Wall`.

## Testing

`MOTOR_DRIVE/motor/tests/test_motor.c` is a standalone host regression suite using C assertions and mocked hardware callbacks. Name new cases `test_<behavior>` and call them from `main()`. Build with a host C compiler, assertions enabled, and `-lm`. The current tree omits the required `parameter_list.h` and `xmath.h` headers and has no CTest target or configured coverage threshold.

## Commits and Pull Requests

Prefer short, imperative subjects such as `Fix PWM sequencing`. In pull requests, explain the behavior change, link the issue when applicable, and report the build presets and any hardware checks you ran.
