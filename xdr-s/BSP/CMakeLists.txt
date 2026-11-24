add_library(bsp STATIC
        math/bsp_math.c
        flash/bsp_mcu_flash.c
        bus/bap_can.c
        enc/bsp_enc_adc.c
        enc/bsp_enc_spi.c
        enc/bsp_enc_tim.c
        enc/abi/enc_abi.c
        enc/hall/enc_hall.c
        enc/sincos/enc_sincos.c
        enc/spi/enc_as5047.c
        enc/spi/enc_mt6816.c
        enc/spi/enc_mt6835.c
        mcu/bsp_mcu.c
        led/bsp_gpio_led.c
        pwm/bsp_mcu_pwm.c
        sample/bsp_mcu_adc.c
        uart/bsp_mcu_uart.c
        bsp_config.h
)

target_include_directories(bsp PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/math"
        "${CMAKE_CURRENT_SOURCE_DIR}/flash"
        "${CMAKE_CURRENT_SOURCE_DIR}/usb"
        "${CMAKE_CURRENT_SOURCE_DIR}/bus"
        "${CMAKE_CURRENT_SOURCE_DIR}/enc"
        "${CMAKE_CURRENT_SOURCE_DIR}/enc/abi"
        "${CMAKE_CURRENT_SOURCE_DIR}/enc/hall"
        "${CMAKE_CURRENT_SOURCE_DIR}/enc/sincos"
        "${CMAKE_CURRENT_SOURCE_DIR}/enc/spi"
        "${CMAKE_CURRENT_SOURCE_DIR}/mcu"
        "${CMAKE_CURRENT_SOURCE_DIR}/led"
        "${CMAKE_CURRENT_SOURCE_DIR}/pwm"
        "${CMAKE_CURRENT_SOURCE_DIR}/sample"
        "${CMAKE_CURRENT_SOURCE_DIR}/uart"
)

target_link_libraries(bsp PUBLIC
        stm32cubemx
        motor_interfaces
        "${PROJECT_SOURCE_DIR}/Middlewares/ST/ARM/DSP/Lib/libarm_cortexM4lf_math.a"
        m
)
